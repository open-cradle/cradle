#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <rpc/this_handler.h>

#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/dll/dll_collection.h>
#include <cradle/inner/introspection/tasklet_impl.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/inner/remote/config.h>
#include <cradle/inner/requests/cast_ctx.h>
#include <cradle/inner/requests/domain.h>
#include <cradle/inner/resolve/seri_lock.h>
#include <cradle/inner/resolve/seri_req.h>
#include <cradle/inner/resolve/util.h>
#include <cradle/inner/service/config_map_from_json.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/rpclib/common/config.h>
#include <cradle/rpclib/server/handlers.h>
#include <cradle/thinknode/service/core.h>

namespace cradle {

thread_pool_guard::thread_pool_guard(int num_available_threads)
    : num_free_threads_{num_available_threads}
{
}

void
thread_pool_guard::claim_thread()
{
    std::scoped_lock lock{mutex_};
    if (num_free_threads_ == 0)
    {
        // Disguise as an error raised by the rpclib library, so that it looks
        // retryable.
        throw std::runtime_error{
            "rpclib: all threads for this request type are busy"};
    }
    num_free_threads_ -= 1;
}

void
thread_pool_guard::release_thread()
{
    std::scoped_lock lock{mutex_};
    num_free_threads_ += 1;
}

thread_pool_claim::thread_pool_claim(thread_pool_guard& guard) : guard_{guard}
{
    guard_.claim_thread();
}

thread_pool_claim::~thread_pool_claim()
{
    guard_.release_thread();
}

rpclib_handler_context::rpclib_handler_context(
    service_config const& config,
    service_core& service,
    spdlog::logger& logger)
    : service_{service},
      testing_{
          config.get_bool_or_default(generic_config_keys::TESTING, false)},
      logger_{logger},
      handler_pool_size_{std::max(
          2,
          static_cast<int>(config.get_number_or_default(
              rpclib_config_keys::REQUEST_CONCURRENCY, 16)))},
      handler_pool_guard_{handler_pool_size_ - 1},
      async_request_pool_size_{static_cast<int>(config.get_number_or_default(
          rpclib_config_keys::REQUEST_CONCURRENCY, 16))},
      async_request_pool_{
          static_cast<BS::concurrency_t>(async_request_pool_size_)}
{
}

thread_pool_claim
rpclib_handler_context::claim_sync_request_thread()
{
    return thread_pool_claim{handler_pool_guard_};
}

static seri_cache_record_lock_t
alloc_cache_record_lock_if_needed(
    rpclib_handler_context& hctx, bool need_record_lock)
{
    if (!need_record_lock)
    {
        return seri_cache_record_lock_t{};
    }
    return hctx.service().alloc_cache_record_lock();
}

// [[noreturn]]
// Throws something that is handled inside the rpclib library
static void
handle_exception(rpclib_handler_context& hctx, std::exception& e)
{
    auto& logger{hctx.logger()};
    logger.error("caught {}", e.what());
    rpc::this_handler().respond_error(e.what());
}

static rpclib_response
resolve_sync(
    rpclib_handler_context& hctx,
    std::string config_json,
    std::string seri_req)
{
    auto& logger{hctx.logger()};
    service_config config{read_config_map_from_json(config_json)};
    auto domain_name
        = config.get_mandatory_string(remote_config_keys::DOMAIN_NAME);
    logger.info("resolve_sync {}: {}", domain_name, seri_req);
    auto need_record_lock{config.get_bool_or_default(
        remote_config_keys::NEED_RECORD_LOCK, false)};
    auto seri_lock{alloc_cache_record_lock_if_needed(hctx, need_record_lock)};
    auto& dom = hctx.service().find_domain(domain_name);
    auto ctx{dom.make_local_sync_context(config)};
    ctx->track_blob_file_writers();
    cppcoro::task<serialized_result> task;
    auto optional_client_tasklet_id
        = config.get_optional_number(remote_config_keys::TASKLET_ID);
    auto* intr_ctx = cast_ctx_to_ptr<introspective_context_intf>(*ctx);
    if (optional_client_tasklet_id && intr_ctx)
    {
        auto* client_tasklet = create_tasklet_tracker(
            hctx.service().the_tasklet_admin(),
            static_cast<int>(*optional_client_tasklet_id));
        intr_ctx->push_tasklet(*client_tasklet);
        task = resolve_serialized_introspective(
            *intr_ctx,
            "rpclib",
            "resolve_sync",
            std::move(seri_req),
            seri_lock);
    }
    else
    {
        task = resolve_serialized_local(*ctx, std::move(seri_req), seri_lock);
    }
    auto seri_result{cppcoro::sync_wait(std::move(task))};
    // TODO try to get rid of .value()
    blob result = seri_result.value();
    logger.info("result {}", result);
    ctx->on_value_complete();
    // TODO if the result references blob files, then create a response_id
    // uniquely identifying the set of those files
    static std::atomic<uint32_t> response_id = 0;
    response_id += 1;
    return rpclib_response{
        response_id, seri_lock.record_id.value(), std::move(result)};
}

rpclib_response
handle_resolve_sync(
    rpclib_handler_context& hctx,
    std::string config_json,
    std::string seri_req)
try
{
    auto claim{hctx.claim_sync_request_thread()};
    // resolve_sync() blocks the handler thread, but thanks to the claim there
    // will be at least one thread left to handle incoming requests.
    return resolve_sync(hctx, std::move(config_json), std::move(seri_req));
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return rpclib_response{};
}

void
handle_ack_response(rpclib_handler_context& hctx, int response_id)
try
{
    auto& logger{hctx.logger()};
    logger.info("ack_response {}", response_id);
    // TODO release the temporary lock on the blob files referenced in
    // response #response_id
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
}

void
handle_mock_http(rpclib_handler_context& hctx, std::string body)
try
{
    auto& session = hctx.service().enable_http_mocking();
    session.set_canned_response(make_http_200_response(body));
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
}

// Resolves an async request, running on a dedicated thread from the
// async_request_pool_.
static void
resolve_async(
    rpclib_handler_context& hctx,
    std::shared_ptr<root_local_async_context_intf> actx,
    std::string seri_req,
    seri_cache_record_lock_t seri_lock)
{
    auto& logger{hctx.logger()};
    if (auto* test_ctx = dynamic_cast<test_context_intf*>(&*actx))
    {
        test_ctx->apply_resolve_async_delay();
    }
    logger.info("resolve_async start");
    // TODO update status to STARTED or so
    try
    {
        blob res
            = cppcoro::sync_wait(resolve_serialized_local(
                                     *actx, std::move(seri_req), seri_lock))
                  .value();
        logger.info("resolve_async done: {}", res);
        actx->set_result(std::move(res));
        actx->set_cache_record_id(seri_lock.record_id);
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
handle_submit_async(
    rpclib_handler_context& hctx,
    std::string config_json,
    std::string seri_req)
try
{
    auto& logger{hctx.logger()};
    service_config config{read_config_map_from_json(config_json)};
    auto domain_name
        = config.get_mandatory_string(remote_config_keys::DOMAIN_NAME);
    logger.info(
        "submit_async {}: {} ...", domain_name, seri_req.substr(0, 10));
    // logger.info("  config_json {}", config_json);
    auto& dom = hctx.service().find_domain(domain_name);
    auto actx{dom.make_local_async_context(config)};
    actx->track_blob_file_writers();
    if (auto* test_ctx = dynamic_cast<test_context_intf*>(&*actx))
    {
        test_ctx->apply_fail_submit_async();
        test_ctx->apply_submit_async_delay();
    }
    actx->using_result();
    hctx.get_async_db().add(actx);
    // TODO update status to SUBMITTED
    // This function should return asap, but its work is done by the blocking
    // resolve_async(), which therefore is being dispatched. BS::thread_pool
    // puts it on a queue that grows as needed, meaning no additional mechanism
    // is needed to keep the server responsive (in contrast to the
    // resolve_sync() situation).
    // TODO actx writes before now should synchronize with the pool thread
    auto need_record_lock{config.get_bool_or_default(
        remote_config_keys::NEED_RECORD_LOCK, false)};
    auto seri_lock{alloc_cache_record_lock_if_needed(hctx, need_record_lock)};
    hctx.async_request_pool().detach_task([&hctx,
                                           actx,
                                           seri_req = std::move(seri_req),
                                           seri_lock = std::move(seri_lock)] {
        resolve_async(hctx, actx, std::move(seri_req), std::move(seri_lock));
    });
    async_id aid = actx->get_id();
    logger.info("async_id {}", aid);
    return aid;
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return async_id{};
}

remote_context_spec_list
handle_get_sub_contexts(rpclib_handler_context& hctx, async_id aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.info("handle_get_sub_contexts {}", aid);
    auto actx{db.find(aid)};
    auto nsubs = actx->get_local_num_subs();
    logger.debug("  {} subs", nsubs);
    remote_context_spec_list result;
    for (decltype(nsubs) ix = 0; ix < nsubs; ++ix)
    {
        auto& sub_actx = actx->get_local_sub(ix);
        logger.debug(
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
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return remote_context_spec_list{};
}

int
handle_get_async_status(rpclib_handler_context& hctx, async_id aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.debug("handle_get_async_status {}", aid);
    auto actx{db.find(aid)};
    auto status = actx->get_status();
    logger.debug("handle_get_async_status -> {}", status);
    return static_cast<int>(status);
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return int{};
}

std::string
handle_get_async_error_message(rpclib_handler_context& hctx, async_id aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.info("handle_get_async_error_message {}", aid);
    auto actx{db.find(aid)};
    auto errmsg = actx->get_error_message();
    logger.info("handle_get_async_error_message -> {}", errmsg);
    return errmsg;
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return std::string{};
}

rpclib_response
handle_get_async_response(rpclib_handler_context& hctx, async_id root_aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.info("handle_get_async_response {}", root_aid);
    auto actx{db.find_root(root_aid)};
    // TODO response_id
    uint32_t response_id = 0;
    return rpclib_response{
        response_id, actx->get_cache_record_id().value(), actx->get_result()};
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return rpclib_response{};
}

int
handle_request_cancellation(rpclib_handler_context& hctx, async_id aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.info("handle_request_cancellation {}", aid);
    auto actx{db.find(aid)};
    actx->request_cancellation();
    return int{};
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return int{};
}

int
handle_finish_async(rpclib_handler_context& hctx, async_id root_aid)
try
{
    auto& db{hctx.get_async_db()};
    auto& logger{hctx.logger()};
    logger.info("handle_finish_async {}", root_aid);
    db.remove_tree(root_aid);
    return int{};
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return int{};
}

tasklet_info_tuple_list
handle_get_tasklet_infos(rpclib_handler_context& hctx, bool include_finished)
try
{
    return make_info_tuples(get_tasklet_infos(
        hctx.service().the_tasklet_admin(), include_finished));
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return tasklet_info_tuple_list{};
}

void
handle_load_shared_library(
    rpclib_handler_context& hctx, std::string dir_path, std::string dll_name)
try
{
    auto& logger{hctx.logger()};
    logger.info("handle_load_shared_library({}, {})", dir_path, dll_name);

    auto& the_dlls{hctx.service().the_dlls()};
    the_dlls.load(dir_path, dll_name);
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
}

void
handle_unload_shared_library(
    rpclib_handler_context& hctx, std::string dll_name)
try
{
    auto& logger{hctx.logger()};
    logger.info("handle_unload_shared_library({})", dll_name);

    auto& the_dlls{hctx.service().the_dlls()};
    the_dlls.unload(dll_name);
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
}

void
handle_clear_unused_mem_cache_entries(rpclib_handler_context& hctx)
try
{
    auto& logger{hctx.logger()};
    logger.info("handle_clear_unused_mem_cache_entries()");

    auto& resources{hctx.service()};
    clear_unused_entries(resources.memory_cache());
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
}

void
handle_release_cache_record_lock(
    rpclib_handler_context& hctx, remote_cache_record_id record_id)
try
{
    auto& logger{hctx.logger()};
    logger.info("handle_release_cache_record_lock({})", record_id.value());

    hctx.service().release_cache_record_lock(record_id);
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
}

int
handle_get_num_contained_calls(rpclib_handler_context& hctx)
try
{
    auto& logger{hctx.logger()};
    logger.info("handle_get_num_contained_calls");
    auto& resources{hctx.service()};
    int num = resources.get_num_contained_calls();
    logger.info("handle_get_num_contained_calls -> {}", num);
    return num;
}
catch (std::exception& e)
{
    handle_exception(hctx, e);
    return int{};
}

} // namespace cradle
