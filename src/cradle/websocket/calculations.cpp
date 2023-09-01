#include <cradle/websocket/calculations.h>

#include <picosha2.h>

// Boost.Crc triggers some warnings on MSVC.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4245)
#pragma warning(disable : 4701)
#include <boost/crc.hpp>
#pragma warning(pop)
#else
#include <boost/crc.hpp>
#endif

#include <cppcoro/when_all.hpp>

#include <cradle/inner/core/sha256_hash_id.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/utilities/errors.h>
#include <cradle/inner/utilities/functional.h>
#include <cradle/thinknode/caching.h>
#include <cradle/thinknode/calc.h>
#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/supervisor.h>
#include <cradle/thinknode/utilities.h>
#include <cradle/typing/core/dynamic.h>
#include <cradle/typing/encodings/msgpack.h>
#include <cradle/typing/encodings/native.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/logging.h>
#include <cradle/websocket/local_calcs.h>
#include <cradle/websocket/server_api.h>

namespace cradle {

namespace uncached {

cppcoro::task<dynamic>
perform_lambda_calc(
    thinknode_request_context ctx,
    lambda_function const& function,
    std::vector<dynamic> args)
{
    auto app = string{"any"};
    auto image = make_thinknode_provider_image_info_with_tag("unused");
    auto pool_name = std::string{"lambda@"} + app;
    co_await get_local_compute_pool_for_image(
        ctx.service, std::make_pair(app, image))
        .schedule();

    auto tasklet
        = create_tasklet_tracker(pool_name, "lambda func", ctx.get_tasklet());
    auto run_guard = tasklet_run(tasklet);
    co_return function.object(std::move(args), tasklet);
}

} // namespace uncached

cppcoro::task<dynamic>
perform_lambda_calc(
    thinknode_request_context ctx,
    lambda_function const& function,
    std::vector<dynamic> args)
{
    string function_name{"lambda_calc"};
    auto cache_key{combine_ids(
        make_id(function_name),
        ref(function.id),
        make_id(natively_encoded_sha256(args)))};

    auto await_guard
        = tasklet_await(ctx.get_tasklet(), function_name, *cache_key);
    co_return co_await cached<dynamic>(
        ctx.service, cache_key, [&](captured_id const&) {
            return uncached::perform_lambda_calc(
                ctx, function, std::move(args));
        });
}

cppcoro::task<std::string>
resolve_calc_to_iss_object(
    thinknode_request_context ctx,
    string context_id,
    std::map<string, calculation_request> const& environment,
    calculation_request request);

cppcoro::task<dynamic>
resolve_calc_to_value(
    thinknode_request_context ctx,
    string context_id,
    std::map<string, calculation_request> const& environment,
    calculation_request request)
{
    auto recursive_call
        = [&](calculation_request request) -> cppcoro::task<dynamic> {
        return resolve_calc_to_value(
            ctx, context_id, environment, std::move(request));
    };
    auto coercive_call = [&](thinknode_type_info const& schema,
                             dynamic value) -> cppcoro::task<dynamic> {
        return coerce_local_calc_result(
            ctx, context_id, schema, std::move(value));
    };

    switch (get_tag(request))
    {
        case calculation_request_tag::REFERENCE:
            co_return co_await get_iss_object(
                ctx, context_id, as_reference(request));
        case calculation_request_tag::VALUE:
            co_return as_value(std::move(request));
        case calculation_request_tag::LAMBDA: {
            std::vector<dynamic> arg_values;
            auto lambda = as_lambda(std::move(request));
            // TODO: Do this in parallel.
            for (auto& arg : lambda.args)
                arg_values.push_back(co_await recursive_call(std::move(arg)));
            co_return co_await perform_lambda_calc(
                ctx, lambda.function, std::move(arg_values));
        }
        case calculation_request_tag::FUNCTION:
            // If the function is specifically requested to be executed
            // remotely in Thinknode, then resolve the calculation to an ISS
            // object and download the associated data.
            if (as_function(request).host
                && *as_function(request).host
                       == execution_host_selection::THINKNODE)
            {
                co_return co_await get_iss_object(
                    ctx,
                    context_id,
                    co_await resolve_calc_to_iss_object(
                        ctx, context_id, environment, std::move(request)));
            }
            // Otherwise, we evaluate the function locally.
            else
            {
                std::vector<dynamic> arg_values;
                auto function = as_function(std::move(request));
                // TODO: Do this in parallel.
                for (auto& arg : function.args)
                {
                    arg_values.push_back(
                        co_await recursive_call(std::move(arg)));
                }
                co_return co_await perform_local_function_calc(
                    ctx,
                    context_id,
                    function.account,
                    function.app,
                    function.name,
                    std::move(arg_values));
            }
        case calculation_request_tag::ARRAY: {
            std::vector<dynamic> values;
            auto array = as_array(std::move(request));
            for (auto& item : array.items)
            {
                spdlog::get("cradle")->info(
                    "array.item_schema: {}",
                    boost::lexical_cast<std::string>(array.item_schema));
                values.push_back(co_await coercive_call(
                    array.item_schema,
                    co_await recursive_call(std::move(item))));
            }
            co_return dynamic(values);
        }
        case calculation_request_tag::ITEM: {
            auto item = as_item(std::move(request));
            co_return co_await coercive_call(
                item.schema,
                cast<dynamic_array>(
                    co_await recursive_call(std::move(item.array)))
                    .at(boost::numeric_cast<size_t>(cast<integer>(
                        co_await recursive_call(std::move(item.index))))));
        }
        case calculation_request_tag::OBJECT: {
            auto object = as_object(std::move(request));
            dynamic_map result;
            for (auto& property : object.properties)
            {
                result[dynamic(property.first)]
                    = co_await recursive_call(property.second);
            }
            co_return co_await coercive_call(object.schema, std::move(result));
        }
        case calculation_request_tag::PROPERTY: {
            auto property = as_property(std::move(request));
            co_return co_await coercive_call(
                property.schema,
                cast<dynamic_map>(
                    co_await recursive_call(std::move(property.object)))
                    .at(cast<string>(
                        co_await recursive_call(std::move(property.field)))));
        }
        case calculation_request_tag::LET: {
            auto let = as_let(std::move(request));
            std::map<string, calculation_request> extended_environment
                = environment;
            for (auto& v : let.variables)
                extended_environment[v.first] = std::move(v.second);
            co_return co_await resolve_calc_to_value(
                ctx, context_id, extended_environment, std::move(let.in));
        }
        case calculation_request_tag::VARIABLE:
            co_return co_await recursive_call(
                environment.at(as_variable(request)));
        case calculation_request_tag::META: {
            auto meta = as_meta(std::move(request));
            auto generated_request = from_dynamic<calculation_request>(
                co_await recursive_call(std::move(meta.generator)));
            co_return co_await coercive_call(
                meta.schema, co_await recursive_call(generated_request));
        }
        case calculation_request_tag::CAST: {
            auto cast = as_cast(std::move(request));
            co_return co_await coercive_call(
                cast.schema, co_await recursive_call(std::move(cast.object)));
        }
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("thinknode_calc_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }
}

cppcoro::task<std::string>
resolve_calc_to_iss_object(
    thinknode_request_context ctx,
    string context_id,
    std::map<string, calculation_request> const& environment,
    calculation_request request)
{
    // For most calculation types, the resolution will be to convert the
    // calculation to a Thinknode calculation in "shallow form" (where all
    // subcalculations are stored as references) and then post that
    // calculation.
    //
    // The following helpers aid in those cases.

    // recurse() is used on subcalculations to reduce them to reference
    // calculations. Note that this is not quite the same interface as
    // resolve_calc_to_iss_object() itself provides, but it's more convenient
    // for most of the cases here that require recursion.
    auto recurse = [&](calculation_request calc)
        -> cppcoro::task<thinknode_calc_request> {
        co_return make_thinknode_calc_request_with_reference(
            co_await resolve_calc_to_iss_object(
                ctx, context_id, environment, std::move(calc)));
    };

    // post_calc() aids in posting the shallow form of the calculation.
    auto post_calc =
        [&](thinknode_calc_request calc) -> cppcoro::shared_task<std::string> {
        return post_calculation(ctx, context_id, std::move(calc));
    };

    // For other types, the resolution will be to simply post the result as a
    // value to Thinknode via this helper.
    auto post_value = [&](dynamic value) -> cppcoro::shared_task<std::string> {
        return post_iss_object(
            ctx,
            context_id,
            make_thinknode_type_info_with_dynamic_type(
                thinknode_dynamic_type()),
            std::move(value));
    };

    switch (get_tag(request))
    {
        case calculation_request_tag::REFERENCE:
            co_return as_reference(std::move(request));
        case calculation_request_tag::VALUE:
            co_return co_await post_value(as_value(std::move(request)));
        case calculation_request_tag::LAMBDA: {
            co_return co_await post_value(co_await resolve_calc_to_value(
                ctx, context_id, environment, std::move(request)));
        }
        case calculation_request_tag::FUNCTION: {
            // If the function is specifically requested to be executed
            // locally, then resolve it locally to a value and upload that to
            // Thinknode.
            if (as_function(request).host
                && *as_function(request).host
                       == execution_host_selection::LOCAL)
            {
                co_return co_await post_value(co_await resolve_calc_to_value(
                    ctx, context_id, environment, std::move(request)));
            }
            // Otherwise, we request Thinknode to evaluate it.
            else
            {
                auto subtasks
                    = map(recurse, std::move(as_function(request).args));
                co_return co_await post_calc(
                    make_thinknode_calc_request_with_function(
                        make_thinknode_function_application(
                            as_function(request).account,
                            as_function(request).app,
                            as_function(request).name,
                            as_function(request).level,
                            co_await cppcoro::when_all(std::move(subtasks)))));
            }
        }
        case calculation_request_tag::ARRAY: {
            auto subtasks = map(recurse, std::move(as_array(request).items));
            co_return co_await post_calc(
                make_thinknode_calc_request_with_array(
                    make_thinknode_array_calc(
                        co_await cppcoro::when_all(std::move(subtasks)),
                        as_array(request).item_schema)));
        }
        case calculation_request_tag::ITEM:
            co_return co_await post_calc(
                make_thinknode_calc_request_with_item(make_thinknode_item_calc(
                    co_await recurse(as_item(request).array),
                    co_await recurse(as_item(request).index),
                    as_item(request).schema)));
        case calculation_request_tag::OBJECT: {
            std::map<string, thinknode_calc_request> properties;
            for (auto& property : as_object(request).properties)
            {
                properties[property.first]
                    = co_await recurse(std::move(property.second));
            }
            co_return co_await post_calc(
                make_thinknode_calc_request_with_object(
                    make_thinknode_object_calc(
                        properties, as_object(request).schema)));
        }
        case calculation_request_tag::PROPERTY:
            co_return co_await post_calc(
                make_thinknode_calc_request_with_property(
                    make_thinknode_property_calc(
                        co_await recurse(as_property(request).object),
                        co_await recurse(as_property(request).field),
                        as_property(request).schema)));
        case calculation_request_tag::LET: {
            auto let = as_let(std::move(request));
            std::map<string, calculation_request> extended_environment
                = environment;
            for (auto& v : let.variables)
                extended_environment[v.first] = std::move(v.second);
            co_return co_await resolve_calc_to_iss_object(
                ctx, context_id, extended_environment, std::move(let.in));
        }
        case calculation_request_tag::VARIABLE:
            co_return co_await resolve_calc_to_iss_object(
                ctx,
                context_id,
                environment,
                environment.at(as_variable(request)));
        case calculation_request_tag::META:
            co_return co_await post_calc(
                make_thinknode_calc_request_with_meta(make_thinknode_meta_calc(
                    co_await recurse(as_meta(request).generator),
                    as_meta(request).schema)));
        case calculation_request_tag::CAST:
            co_return co_await post_calc(make_thinknode_calc_request_with_cast(
                make_thinknode_cast_request(
                    as_cast(request).schema,
                    co_await recurse(as_cast(request).object))));
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("thinknode_calc_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }
}

cppcoro::task<dynamic>
resolve_calc_to_value(
    thinknode_request_context ctx,
    string context_id,
    calculation_request request)
{
    co_return co_await resolve_calc_to_value(
        ctx, context_id, {}, std::move(request));
}

cppcoro::task<std::string>
resolve_calc_to_iss_object(
    thinknode_request_context ctx,
    string context_id,
    calculation_request request)
{
    co_return co_await resolve_calc_to_iss_object(
        ctx, context_id, {}, std::move(request));
}

calculation_request
as_generic_calc(thinknode_calc_request const& request)
{
    switch (get_tag(request))
    {
        case thinknode_calc_request_tag::REFERENCE:
            return make_calculation_request_with_reference(
                as_reference(request));
        case thinknode_calc_request_tag::VALUE:
            return make_calculation_request_with_value(as_value(request));
        case thinknode_calc_request_tag::FUNCTION:
            return make_calculation_request_with_function(
                make_function_application(
                    as_function(request).account,
                    as_function(request).app,
                    as_function(request).name,
                    execution_host_selection::THINKNODE,
                    as_function(request).level,
                    map(as_generic_calc, as_function(request).args)));
        case thinknode_calc_request_tag::ARRAY:
            return make_calculation_request_with_array(make_array_calc_request(
                map(as_generic_calc, as_array(request).items),
                as_array(request).item_schema));
        case thinknode_calc_request_tag::ITEM:
            return make_calculation_request_with_item(make_item_calc_request(
                as_generic_calc(as_item(request).array),
                as_generic_calc(as_item(request).index),
                as_item(request).schema));
        case thinknode_calc_request_tag::OBJECT:
            return make_calculation_request_with_object(
                make_object_calc_request(
                    map(as_generic_calc, as_object(request).properties),
                    as_object(request).schema));
        case thinknode_calc_request_tag::PROPERTY:
            return make_calculation_request_with_property(
                make_property_calc_request(
                    as_generic_calc(as_property(request).object),
                    as_generic_calc(as_property(request).field),
                    as_property(request).schema));
        case thinknode_calc_request_tag::LET:
            return make_calculation_request_with_let(make_let_calc_request(
                map(as_generic_calc, as_let(request).variables),
                as_generic_calc(as_let(request).in)));
        case thinknode_calc_request_tag::VARIABLE:
            return make_calculation_request_with_variable(
                as_variable(request));
        case thinknode_calc_request_tag::META:
            return make_calculation_request_with_meta(make_meta_calc_request(
                as_generic_calc(as_meta(request).generator),
                as_meta(request).schema));
        case thinknode_calc_request_tag::CAST:
            return make_calculation_request_with_cast(make_cast_calc_request(
                as_cast(request).schema,
                as_generic_calc(as_cast(request).object)));
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("thinknode_calc_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }
}

} // namespace cradle
