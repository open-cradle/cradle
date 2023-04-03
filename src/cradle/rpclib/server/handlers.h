#ifndef CRADLE_RPCLIB_SERVER_HANDLERS_H
#define CRADLE_RPCLIB_SERVER_HANDLERS_H

#include <string>

#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>
#include <thread-pool/thread_pool.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/rpclib/common/common.h>
#include <cradle/typing/service/core.h>

namespace cradle {

class rpclib_handler_context
{
 public:
    rpclib_handler_context(service_core& service, spdlog::logger& logger);

    service_core&
    service()
    {
        return service_;
    }

    spdlog::logger&
    logger()
    {
        return logger_;
    }

    thread_pool&
    async_pool()
    {
        return async_pool_;
    }

    async_db&
    get_async_db()
    {
        // async_db existence ensured by run_server()
        return *service_.get_async_db();
    }

 private:
    service_core& service_;
    spdlog::logger& logger_;
    thread_pool async_pool_;
};

cppcoro::task<rpclib_response>
handle_resolve_sync(
    rpclib_handler_context& hctx,
    std::string domain_name,
    std::string seri_req);

cppcoro::task<void>
handle_ack_response(rpclib_handler_context& hctx, int response_id);

cppcoro::task<void>
handle_mock_http(rpclib_handler_context& hctx, std::string const& body);

async_id
handle_submit_async(
    rpclib_handler_context& hctx,
    std::string const& domain_name,
    std::string const& seri_req);

remote_context_spec_list
handle_get_sub_contexts(rpclib_handler_context& hctx, async_id aid);

int
handle_get_async_status(rpclib_handler_context& hctx, async_id aid);

rpclib_response
handle_get_async_response(rpclib_handler_context& hctx, async_id root_aid);

int
handle_request_cancellation(rpclib_handler_context& hctx, async_id aid);

int
handle_finish_async(rpclib_handler_context& hctx, async_id root_aid);

} // namespace cradle

#endif
