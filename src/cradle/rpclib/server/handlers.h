#ifndef CRADLE_RPCLIB_SERVER_HANDLERS_H
#define CRADLE_RPCLIB_SERVER_HANDLERS_H

#include <memory>
#include <mutex>
#include <string>

#include <BS_thread_pool.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/remote/types.h>
#include <cradle/rpclib/common/common.h>
#include <cradle/thinknode/service/core.h>

namespace cradle {

// Guards a thread pool, ensuring that the number of claimed threads never
// exceeds the availability.
class thread_pool_guard
{
 public:
    thread_pool_guard(int num_available_threads);

    // Claims a thread; throws if none available
    void
    claim_thread();

    // Releases a claimed thread
    void
    release_thread();

 private:
    std::mutex mutex_;
    int num_free_threads_;
};

// Claim on a thread from a pool.
// RAII class (the thread being the allocated resource).
// Must be created before actually allocating a thread from a pool.
// Must be destroyed just before the thread finishes its job.
class thread_pool_claim
{
 public:
    thread_pool_claim(thread_pool_guard& guard);

    ~thread_pool_claim();

 private:
    thread_pool_guard& guard_;
};

// Context shared by the request handler threads.
class rpclib_handler_context
{
 public:
    rpclib_handler_context(
        service_config const& config,
        service_core& service,
        spdlog::logger& logger);

    service_core&
    service()
    {
        return service_;
    }

    bool
    testing() const
    {
        return testing_;
    }

    spdlog::logger&
    logger()
    {
        return logger_;
    }

    BS::thread_pool&
    async_request_pool()
    {
        return async_request_pool_;
    }

    async_db&
    get_async_db()
    {
        // async_db existence ensured by run_server()
        return *service_.get_async_db();
    }

    int
    handler_pool_size() const
    {
        return handler_pool_size_;
    }

    // Claims a thread for handling a resolve_sync request
    // (the only type of request that could block a handler thread for an
    // undeterminate time).
    thread_pool_claim
    claim_sync_request_thread();

 private:
    service_core& service_;
    bool testing_;
    spdlog::logger& logger_;

    // Each incoming request is handled by a separate thread from a pool
    // containing handler_pool_size_ threads.
    // A handler thread handles a short request (taking little time to handle),
    // or a potentially long resolve_sync one. If all handler threads would be
    // busy resolving a resolve_sync request, the server is unresponsive until
    // the first thread finishes.
    // To prevent this, the number of threads available for the resolve_sync
    // requests is handler_pool_size_ - 1, so that always one thread is left to
    // handle short requests, and the server remains responsive. This means
    // that handler_pool_size_ must be at least 2.
    // If a resolve_sync request comes in while no threads are available, the
    // request immediately fails with a "busy" error.
    // The thread pool itself is created in run_server().
    int const handler_pool_size_;
    thread_pool_guard handler_pool_guard_;

    // A handler thread dispatches a resolve_async request to an async thread
    // from a pool of async_request_pool_size_ threads. Dispatching happens
    // via a request queue of unbounded size. Thus, even if all async threads
    // would be busy, the server stays responsive.
    int const async_request_pool_size_;
    BS::thread_pool async_request_pool_;
};

rpclib_response
handle_resolve_sync(
    rpclib_handler_context& hctx,
    std::string config_json,
    std::string seri_req);

void
handle_ack_response(rpclib_handler_context& hctx, int response_id);

void
handle_mock_http(rpclib_handler_context& hctx, std::string body);

int
handle_store_request(
    rpclib_handler_context& hctx,
    std::string storage_name,
    std::string key,
    std::string seri_req);

async_id
handle_submit_async(
    rpclib_handler_context& hctx,
    std::string config_json,
    std::string seri_req);

async_id
handle_submit_stored(
    rpclib_handler_context& hctx,
    std::string config_json,
    std::string storage_name,
    std::string key);

remote_context_spec_list
handle_get_sub_contexts(rpclib_handler_context& hctx, async_id aid);

int
handle_get_async_status(rpclib_handler_context& hctx, async_id aid);

std::string
handle_get_async_error_message(rpclib_handler_context& hctx, async_id aid);

rpclib_response
handle_get_async_response(rpclib_handler_context& hctx, async_id root_aid);

int
handle_request_cancellation(rpclib_handler_context& hctx, async_id aid);

int
handle_finish_async(rpclib_handler_context& hctx, async_id root_aid);

tasklet_info_tuple_list
handle_get_tasklet_infos(rpclib_handler_context& hctx, bool include_finished);

void
handle_load_shared_library(
    rpclib_handler_context& hctx, std::string dir_path, std::string dll_name);

void
handle_unload_shared_library(
    rpclib_handler_context& hctx, std::string dll_name);

void
handle_clear_unused_mem_cache_entries(rpclib_handler_context& hctx);

void
handle_release_cache_record_lock(
    rpclib_handler_context& hctx, remote_cache_record_id record_id);

int
handle_get_num_contained_calls(rpclib_handler_context& hctx);

rpclib_essentials
handle_get_essentials(rpclib_handler_context& hctx, async_id aid);

} // namespace cradle

#endif
