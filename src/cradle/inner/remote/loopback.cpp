#include <stdexcept>

#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/dll/dll_collection.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/inner/remote/config.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/cast_ctx.h>
#include <cradle/inner/requests/domain.h>
#include <cradle/inner/requests/test_context.h>
#include <cradle/inner/resolve/seri_req.h>
#include <cradle/inner/resolve/util.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

loopback_service::loopback_service(std::unique_ptr<inner_resources> resources)
    : resources_{std::move(resources)},
      testing_{resources_->config().get_bool_or_default(
          generic_config_keys::TESTING, false)},
      logger_{ensure_logger("loopback")},
      async_pool_{static_cast<BS::concurrency_t>(
          resources_->config().get_number_or_default(
              loopback_config_keys::ASYNC_CONCURRENCY, 16))}
{
    // TODO enable introspection only on demand
    introspection_set_capturing_enabled(resources_->the_tasklet_admin(), true);
}

std::string
loopback_service::name() const
{
    return "loopback";
}

spdlog::logger&
loopback_service::get_logger()
{
    return *logger_;
}

serialized_result
loopback_service::resolve_sync(service_config config, std::string seri_req)
{
    auto domain_name{
        config.get_mandatory_string(remote_config_keys::DOMAIN_NAME)};
    auto& dom = resources_->find_domain(domain_name);
    auto ctx{dom.make_local_sync_context(config)};
    ctx->track_blob_file_writers();
    auto optional_client_tasklet_id
        = config.get_optional_number(remote_config_keys::TASKLET_ID);
    auto* intr_ctx = cast_ctx_to_ptr<introspective_context_intf>(*ctx);
    bool introspective = optional_client_tasklet_id && intr_ctx;
    logger_->debug(
        "resolve_sync{} {}: request {}",
        introspective ? " (introspective)" : "",
        domain_name,
        seri_req);
    auto need_record_lock{config.get_bool_or_default(
        remote_config_keys::NEED_RECORD_LOCK, false)};
    auto seri_lock{alloc_cache_record_lock_if_needed(need_record_lock)};
    cppcoro::task<serialized_result> task;
    if (introspective)
    {
        auto* client_tasklet = create_tasklet_tracker(
            resources_->the_tasklet_admin(),
            static_cast<int>(*optional_client_tasklet_id));
        intr_ctx->push_tasklet(*client_tasklet);
        task = resolve_serialized_introspective(
            *intr_ctx,
            "loopback",
            "resolve_sync",
            std::move(seri_req),
            seri_lock);
    }
    else
    {
        task = resolve_serialized_local(*ctx, std::move(seri_req), seri_lock);
    }
    auto result{cppcoro::sync_wait(std::move(task))};
    // Not really needed for loopback, but mimics rpclib server and improves
    // code coverage
    ctx->on_value_complete();
    logger_->debug("response {}", result.value());
    return result;
}

static void
resolve_async(
    loopback_service* loopback,
    std::shared_ptr<root_local_async_context_intf> actx,
    std::string seri_req,
    seri_cache_record_lock_t seri_lock,
    bool introspective)
{
    auto& logger{loopback->get_logger()};
    if (auto* test_ctx = dynamic_cast<test_context_intf*>(&*actx))
    {
        test_ctx->apply_resolve_async_delay();
    }
    logger.info(
        "resolve_async start{}", introspective ? " (introspective)" : "");
    // TODO update status to STARTED or so
    cppcoro::task<serialized_result> task;
    if (introspective)
    {
        task = resolve_serialized_introspective(
            cast_ctx_to_ref<introspective_context_intf>(*actx),
            "loopback",
            "resolve_async",
            std::move(seri_req),
            seri_lock);
    }
    else
    {
        task = resolve_serialized_local(*actx, std::move(seri_req), seri_lock);
    }
    try
    {
        blob res = cppcoro::sync_wait(task).value();
        logger.info("resolve_async done: {}", res);
        actx->set_result(std::move(res));
        actx->set_cache_record_id(seri_lock.record_id);
        // Not really needed for loopback, but mimics rpclib server and
        // improves code coverage
        actx->on_value_complete();
    }
    catch (async_cancelled const&)
    {
        logger.warn("resolve_async: caught async_cancelled");
        actx->update_status(async_status::CANCELLED);
    }
    catch (std::exception& e)
    {
        logger.warn("resolve_async: caught error {}", e.what());
        actx->update_status_error(e.what());
    }
}

async_id
loopback_service::submit_async(service_config config, std::string seri_req)
{
    auto domain_name
        = config.get_mandatory_string(remote_config_keys::DOMAIN_NAME);
    logger_->info(
        "submit_async {}: {} ...", domain_name, seri_req.substr(0, 10));
    auto& dom = resources_->find_domain(domain_name);
    auto actx{dom.make_local_async_context(config)};
    actx->track_blob_file_writers();
    if (auto* test_ctx = dynamic_cast<test_context_intf*>(&*actx))
    {
        test_ctx->apply_fail_submit_async();
        test_ctx->apply_submit_async_delay();
    }
    actx->using_result();
    resources_->ensure_async_db();
    get_async_db().add(actx);
    // TODO update status to SUBMITTED
    // This function should return asap, but cppcoro::sync_wait() is blocking,
    // so need to dispatch to another thread.
    bool introspective{false};
    auto optional_client_tasklet_id
        = config.get_optional_number(remote_config_keys::TASKLET_ID);
    auto* intr_ctx = cast_ctx_to_ptr<introspective_context_intf>(*actx);
    if (optional_client_tasklet_id && intr_ctx)
    {
        auto* client_tasklet = create_tasklet_tracker(
            resources_->the_tasklet_admin(),
            static_cast<int>(*optional_client_tasklet_id));
        intr_ctx->push_tasklet(*client_tasklet);
        introspective = true;
    }
    auto need_record_lock{config.get_bool_or_default(
        remote_config_keys::NEED_RECORD_LOCK, false)};
    auto seri_lock{alloc_cache_record_lock_if_needed(need_record_lock)};
    async_pool_.detach_task([=, this] {
        resolve_async(
            this,
            actx,
            std::move(seri_req),
            std::move(seri_lock),
            introspective);
    });
    async_id aid = actx->get_id();
    logger_->info("async_id {}", aid);
    return aid;
}

remote_context_spec_list
loopback_service::get_sub_contexts(async_id aid)
{
    logger_->info("handle_get_sub_contexts {}", aid);
    auto actx{get_async_db().find(aid)};
    auto nsubs = actx->get_local_num_subs();
    logger_->debug("  {} subs", nsubs);
    remote_context_spec_list result;
    for (decltype(nsubs) ix = 0; ix < nsubs; ++ix)
    {
        auto& sub_actx = actx->get_local_sub(ix);
        logger_->debug(
            "  sub {}: id {} ({}) {}",
            ix,
            sub_actx.get_id(),
            sub_actx.is_req() ? "REQ" : "VAL",
            sub_actx.get_status());
        result.push_back(
            remote_context_spec{sub_actx.get_id(), sub_actx.is_req()});
    }
    return result;
}

async_status
loopback_service::get_async_status(async_id aid)
{
    logger_->debug("handle_get_async_status {}", aid);
    auto actx{get_async_db().find(aid)};
    auto status = actx->get_status();
    logger_->debug("handle_get_async_status -> {}", status);
    return status;
}

std::string
loopback_service::get_async_error_message(async_id aid)
{
    logger_->info("handle_get_async_error_message {}", aid);
    auto actx{get_async_db().find(aid)};
    auto errmsg = actx->get_error_message();
    logger_->info("handle_get_async_error_message -> {}", errmsg);
    return errmsg;
}

serialized_result
loopback_service::get_async_response(async_id root_aid)
{
    logger_->info("handle_get_async_response {}", root_aid);
    auto actx{get_async_db().find_root(root_aid)};
    return serialized_result{actx->get_result(), actx->get_cache_record_id()};
}

void
loopback_service::request_cancellation(async_id aid)
{
    logger_->info("handle_request_cancellation {}", aid);
    auto actx{get_async_db().find(aid)};
    actx->request_cancellation();
}

void
loopback_service::finish_async(async_id root_aid)
{
    logger_->info("handle_finish_async {}", root_aid);
    get_async_db().remove_tree(root_aid);
}

tasklet_info_list
loopback_service::get_tasklet_infos(bool include_finished)
{
    logger_->info("get_tasklet_infos {}", include_finished);
    return cradle::get_tasklet_infos(
        resources_->the_tasklet_admin(), include_finished);
}

void
loopback_service::load_shared_library(
    std::string dir_path, std::string dll_name)
{
    logger_->info("load_shared_library {} {}", dir_path, dll_name);
    resources_->the_dlls().load(dir_path, dll_name);
}

void
loopback_service::unload_shared_library(std::string dll_name)
{
    logger_->info("unload_shared_library {}", dll_name);
    resources_->the_dlls().unload(dll_name);
}

void
loopback_service::mock_http(std::string const& response_body)
{
    auto& session = resources_->enable_http_mocking();
    session.set_canned_response(make_http_200_response(response_body));
}

void
loopback_service::clear_unused_mem_cache_entries()
{
    logger_->info("clear_unused_mem_cache_entries");
    clear_unused_entries(resources_->memory_cache());
}

void
loopback_service::release_cache_record_lock(remote_cache_record_id record_id)
{
    resources_->release_cache_record_lock(record_id);
}

int
loopback_service::get_num_contained_calls() const
{
    return resources_->get_num_contained_calls();
}

async_db&
loopback_service::get_async_db()
{
    auto adb{resources_->get_async_db()};
    if (!adb)
    {
        throw std::logic_error{"loopback service has no async_db"};
    }
    return *adb;
}

seri_cache_record_lock_t
loopback_service::alloc_cache_record_lock_if_needed(bool need_record_lock)
{
    if (!need_record_lock)
    {
        return seri_cache_record_lock_t{};
    }
    return resources_->alloc_cache_record_lock();
}

} // namespace cradle
