#ifndef CRADLE_RPCLIB_SERVER_HANDLERS_H
#define CRADLE_RPCLIB_SERVER_HANDLERS_H

#include <string>

#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/core/type_definitions.h>
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

 private:
    service_core& service_;
    spdlog::logger& logger_;
};

cppcoro::task<rpclib_response>
handle_resolve_sync(
    rpclib_handler_context& hctx,
    std::string const& domain_name,
    std::string const& seri_req);

cppcoro::task<void>
handle_ack_response(rpclib_handler_context& hctx, int response_id);

cppcoro::task<void>
handle_mock_http(rpclib_handler_context& hctx, std::string const& body);

} // namespace cradle

#endif
