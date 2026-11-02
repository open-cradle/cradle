#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <catch2/catch.hpp>
#include <cereal/types/string.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>

#include "../../support/inner_service.h"
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/cereal.h>
#include <cradle/inner/encodings/msgpack_value.h>
#include <cradle/inner/requests/containment_data.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/resolve/seri_req.h>
#include <cradle/inner/resolve/seri_result.h>
#include <cradle/inner/service/resources.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][pool][plain_args]";

request_uuid
make_test_uuid(std::string const& ext)
{
    return request_uuid{fmt::format("{}-{}", tag, ext)};
}

// Main request's identity coroutine: returns its single argument unchanged, so
// the resolved value is exactly the value that travelled in the job.
cppcoro::task<int>
echo_int(context_intf&, int x)
{
    co_return x;
}

cppcoro::task<std::string>
echo_string(context_intf&, std::string x)
{
    co_return x;
}

cppcoro::task<blob>
echo_blob(context_intf&, blob x)
{
    co_return x;
}

// Subrequest functions. On the pool plain-args path these are never invoked
// (the resolved value travels in the job and the loader rebuilds an identity
// request around it); they exist only to give the registered subrequest a
// valid stored function.
cppcoro::task<int>
make_int(context_intf&)
{
    co_return 0;
}

cppcoro::task<std::string>
make_string(context_intf&)
{
    co_return std::string{};
}

cppcoro::task<blob>
make_blob_sub(context_intf&)
{
    co_return blob{};
}

// Serializes a value exactly as the pool input path does before a job runs:
// each resolved argument value is serialized with serialize_value, and the
// worker later deserializes those same bytes.
template<typename V>
blob
to_input(V const& value)
{
    bool constexpr allow_blob_files{false};
    return serialize_value(value, allow_blob_files);
}

// Writes the job's inputs as the "args" tuple of the serialized request,
// mirroring local_worker's args_writer: each input is a serialized value blob
// stored under cereal's "tuple_elementN" name.
struct args_writer
{
    std::vector<blob> const& inputs;

    template<typename Archive>
    void
    save(Archive& archive) const
    {
        for (std::size_t i = 0; i != inputs.size(); ++i)
        {
            archive(
                cereal::make_nvp(
                    "tuple_element" + std::to_string(i), inputs[i]));
        }
    }
};

// Reproduces the exact serialized request the local worker builds for a pooled
// leaf (build_seri_req): the derived "+pool_plain_args" variant uuid, the
// default_retrier state and introspection title that retrying/introspective
// leaves consume (ignored by others), no containment, the uniform value-blob
// args, and no pool name.
std::string
build_pool_plain_args_seri_req(
    std::string const& provider, std::vector<blob> const& inputs)
{
    std::stringstream os;
    {
        JSONRequestOutputArchive oarchive(os);
        std::string variant_uuid{pool_variant_uuid_str(provider)};
        oarchive(cereal::make_nvp("uuid", variant_uuid));
        oarchive(cereal::make_nvp("base_millis", int64_t{100}));
        oarchive(cereal::make_nvp("max_attempts", 9));
        oarchive(cereal::make_nvp("title", std::string{"pooled"}));
        containment_data::save_nothing(oarchive);
        oarchive(cereal::make_nvp("args", args_writer{inputs}));
        oarchive(cereal::make_nvp("has_pool_name", false));
    }
    return os.str();
}

// Drives the real pool plain-args load path end to end for a main request whose
// single argument is a function_request subrequest carrying default_retrier:
// register the main request (which auto-registers the pool plain-args variant
// resolver), build the worker's value-blob seri_req, resolve it through
// resolve_serialized_local (which dispatches to the variant resolver -> load
// _plain_args -> rebuild_pool_plain_arg's is_function_request_v branch), and
// assert the resolved value equals the value that travelled in the job.
template<typename V, typename MainFn, typename SubFn>
void
check_default_retrier_pool_plain_arg(
    std::string const& ext, V const& value, MainFn main_fn, SubFn sub_fn)
{
    using main_props_t = request_props<
        caching_level_type::none,
        request_function_t::coro,
        false,
        no_retrier>;
    using sub_props_t = request_props<
        caching_level_type::none,
        request_function_t::coro,
        false,
        default_retrier>;

    auto resources{make_inner_test_resources()};
    non_caching_request_resolution_context ctx{*resources};
    seri_catalog cat{resources->get_seri_registry()};

    auto sub_req{rq_function(sub_props_t{make_test_uuid(ext + "-sub")}, sub_fn)};
    static_assert(std::is_same_v<
                  typename decltype(sub_req)::retrier_type,
                  default_retrier>);

    // normalize_arg returns the function_request subrequest as-is, so the main
    // request's registered argument type carries default_retrier.
    auto main_req{rq_function(
        main_props_t{make_test_uuid(ext + "-main")},
        main_fn,
        normalize_arg<V, main_props_t>(sub_req))};
    cat.register_resolver(main_req);

    std::string const provider{main_req.get_essentials()->uuid_str};
    std::string const seri_req{
        build_pool_plain_args_seri_req(provider, {to_input(value)})};

    auto seri_resp{cppcoro::sync_wait(resolve_serialized_local(ctx, seri_req))};
    V const resp{deserialize_value<V>(seri_resp.value())};
    seri_resp.on_deserialized();
    REQUIRE(resp == value);
}

} // namespace

TEST_CASE(
    "pool plain-args resolve of a default_retrier function_request arg", tag)
{
    // The newly-enabled case: the registered Arg's retrier is default_retrier
    // (as for Thinknode-style requests). Exercised across the value types in
    // play plus a native scalar, through the real variant-resolver dispatch.
    check_default_retrier_pool_plain_arg<int>(
        "int", 42, echo_int, make_int);
    check_default_retrier_pool_plain_arg<std::string>(
        "string", std::string{"hello"}, echo_string, make_string);
    check_default_retrier_pool_plain_arg<blob>(
        "blob", make_blob("world"), echo_blob, make_blob_sub);
}
