#include <cradle/external/external_api_impl.h>
#include <cradle/external_api.h>
#include <cradle/inner/service/config.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache.h>
#include <cradle/thinknode/calc.h>
#include <cradle/thinknode/iam.h>
#include <cradle/thinknode/iss.h>
#include <cradle/websocket/calculations.h>
#include <cradle/websocket/server_api.h>
#include <cradle/websocket/types.hpp>

namespace cradle {

namespace external {

static thinknode_request_context
make_thinknode_request_context(api_session& session, char const* title)
{
    static string const pool_name("ext");
    auto tasklet{create_tasklet_tracker(pool_name, title)};
    return thinknode_request_context{
        session.impl().get_service_core(),
        session.impl().get_thinknode_session(),
        tasklet};
}

api_service::api_service(api_service_config const& config)
    : pimpl_{std::make_unique<api_service_impl>(config)}
{
}

api_service::api_service(api_service&& that) : pimpl_{std::move(that.pimpl_)}
{
}

api_service::~api_service()
{
}

api_service
start_service(api_service_config const& config)
{
    return api_service(config);
}

static void
set_optional_map_entry(
    cradle::service_config_map& config_map,
    std::string const& key,
    std::optional<std::size_t> optional_value)
{
    if (optional_value)
    {
        config_map[key] = *optional_value;
    }
}

static void
set_optional_map_entry(
    cradle::service_config_map& config_map,
    std::string const& key,
    std::optional<int> optional_value)
{
    if (optional_value)
    {
        config_map[key] = static_cast<std::size_t>(*optional_value);
    }
}

static void
set_optional_map_entry(
    cradle::service_config_map& config_map,
    std::string const& key,
    std::optional<std::string> optional_value)
{
    if (optional_value)
    {
        config_map[key] = *optional_value;
    }
}

cradle::service_config
make_service_config(api_service_config const& config)
{
    cradle::service_config_map config_map;
    set_optional_map_entry(
        config_map,
        inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT,
        config.memory_cache_unused_size_limit);
    config_map[inner_config_keys::DISK_CACHE_FACTORY]
        = local_disk_cache_config_values::PLUGIN_NAME;
    set_optional_map_entry(
        config_map,
        local_disk_cache_config_keys::DIRECTORY,
        config.disk_cache_directory);
    set_optional_map_entry(
        config_map,
        local_disk_cache_config_keys::SIZE_LIMIT,
        config.disk_cache_size_limit);
    set_optional_map_entry(
        config_map,
        typing_config_keys::REQUEST_CONCURRENCY,
        config.request_concurrency);
    set_optional_map_entry(
        config_map,
        typing_config_keys::COMPUTE_CONCURRENCY,
        config.compute_concurrency);
    set_optional_map_entry(
        config_map,
        typing_config_keys::HTTP_CONCURRENCY,
        config.http_concurrency);

    return cradle::service_config(config_map);
}

api_service_impl::api_service_impl(api_service_config const& config)
    : service_core_{make_service_config(config)}
{
}

api_session::api_session(
    api_service& service, api_thinknode_session_config const& config)
    : pimpl_{std::make_unique<api_session_impl>(service.impl(), config)}
{
}

api_session::api_session(api_session&& that) : pimpl_{std::move(that.pimpl_)}
{
}

api_session::~api_session()
{
}

cradle::service_core&
get_service_core(api_session& session)
{
    return session.impl().get_service_core();
}

api_session
start_session(api_service& service, api_thinknode_session_config const& config)
{
    return api_session(service, config);
}

api_session_impl::api_session_impl(
    api_service_impl& service, api_thinknode_session_config const& config)
    : service_{service},
      thinknode_session_{
          cradle::make_thinknode_session(config.api_url, config.access_token)}
{
}

cppcoro::task<std::string>
get_context_id(api_session& session, std::string realm)
{
    auto ctx{make_thinknode_request_context(session, "get_context_id")};
    // The lifetime of the tasklet_run object must end after the
    // cradle::get_context_id() coroutine has finished; meaning the current
    // function has to be a coroutine, too.
    auto run_guard{tasklet_run(ctx.get_tasklet())};
    co_return co_await cradle::get_context_id(
        std::move(ctx), std::move(realm));
}

cppcoro::shared_task<blob>
get_iss_object(
    api_session& session,
    std::string context_id,
    std::string object_id,
    bool ignore_upgrades)
{
    auto ctx{make_thinknode_request_context(session, "get_iss_object")};
    auto run_guard{tasklet_run(ctx.get_tasklet())};
    co_return co_await cradle::get_iss_blob(
        std::move(ctx),
        std::move(context_id),
        std::move(object_id),
        ignore_upgrades);
}

cppcoro::shared_task<std::string>
resolve_iss_object_to_immutable(
    api_session& session,
    std::string context_id,
    std::string object_id,
    bool ignore_upgrades)
{
    auto ctx{make_thinknode_request_context(
        session, "resolve_iss_object_to_immutable")};
    auto run_guard{tasklet_run(ctx.get_tasklet())};
    co_return co_await cradle::resolve_iss_object_to_immutable(
        std::move(ctx),
        std::move(context_id),
        std::move(object_id),
        ignore_upgrades);
}

cppcoro::shared_task<std::map<std::string, std::string>>
get_iss_object_metadata(
    api_session& session, std::string context_id, std::string object_id)
{
    auto ctx{
        make_thinknode_request_context(session, "get_iss_object_metadata")};
    auto run_guard{tasklet_run(ctx.get_tasklet())};
    co_return co_await cradle::get_iss_object_metadata(
        std::move(ctx), std::move(context_id), std::move(object_id));
}

cppcoro::shared_task<std::string>
post_iss_object(
    api_session& session,
    std::string context_id,
    std::string schema,
    blob object_data)
{
    auto ctx{make_thinknode_request_context(session, "post_iss_object")};
    auto run_guard{tasklet_run(ctx.get_tasklet())};
    co_return co_await cradle::post_iss_object(
        std::move(ctx),
        std::move(context_id),
        cradle::parse_url_type_string(schema),
        object_data);
}

cppcoro::task<>
copy_iss_object(
    api_session& session,
    std::string source_context_id,
    std::string destination_context_id,
    std::string object_id)
{
    auto ctx{make_thinknode_request_context(session, "copy_iss_object")};
    auto run_guard{tasklet_run(ctx.get_tasklet())};
    auto source_bucket
        = co_await cradle::get_context_bucket(ctx, source_context_id);
    co_await cradle::deeply_copy_iss_object(
        std::move(ctx),
        std::move(source_bucket),
        std::move(source_context_id),
        std::move(destination_context_id),
        std::move(object_id));
}

cppcoro::task<>
copy_calculation(
    api_session& session,
    std::string source_context_id,
    std::string destination_context_id,
    std::string calculation_id)
{
    auto ctx{make_thinknode_request_context(session, "copy_calculation")};
    auto run_guard{tasklet_run(ctx.get_tasklet())};
    auto source_bucket
        = co_await cradle::get_context_bucket(ctx, source_context_id);
    co_await cradle::deeply_copy_calculation(
        std::move(ctx),
        std::move(source_bucket),
        std::move(source_context_id),
        std::move(destination_context_id),
        std::move(calculation_id));
}

cppcoro::task<dynamic>
resolve_calc_to_value(
    api_session& session, string context_id, calculation_request request)
{
    auto ctx{make_thinknode_request_context(session, "resolve_calc_to_value")};
    auto run_guard{tasklet_run(ctx.get_tasklet())};
    co_return co_await cradle::resolve_calc_to_value(
        std::move(ctx), std::move(context_id), std::move(request));
}

cppcoro::task<std::string>
resolve_calc_to_iss_object(
    api_session& session, string context_id, calculation_request request)
{
    auto ctx{
        make_thinknode_request_context(session, "resolve_calc_to_iss_object")};
    auto run_guard{tasklet_run(ctx.get_tasklet())};
    co_return co_await cradle::resolve_calc_to_iss_object(
        std::move(ctx), std::move(context_id), std::move(request));
}

cppcoro::task<calculation_request>
retrieve_calculation_request(
    api_session& session, std::string context_id, std::string calculation_id)
{
    auto ctx{make_thinknode_request_context(
        session, "retrieve_calculation_request")};
    auto run_guard{tasklet_run(ctx.get_tasklet())};
    co_return as_generic_calc(co_await cradle::retrieve_calculation_request(
        std::move(ctx), std::move(context_id), std::move(calculation_id)));
}

} // namespace external

} // namespace cradle
