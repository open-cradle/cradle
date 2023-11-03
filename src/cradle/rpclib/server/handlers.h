#ifndef CRADLE_RPCLIB_SERVER_HANDLERS_H
#define CRADLE_RPCLIB_SERVER_HANDLERS_H

#include <string>

#include <BS_thread_pool.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/rpclib/common/common.h>
#include <cradle/thinknode/service/core.h>

namespace cradle {

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
    request_pool()
    {
        return request_pool_;
    }

    async_db&
    get_async_db()
    {
        // async_db existence ensured by run_server()
        return *service_.get_async_db();
    }

 private:
    service_core& service_;
    bool testing_;
    spdlog::logger& logger_;
    BS::thread_pool request_pool_;
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

async_id
handle_submit_async(
    rpclib_handler_context& hctx,
    std::string config_json,
    std::string seri_req);

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

} // namespace cradle

#endif
