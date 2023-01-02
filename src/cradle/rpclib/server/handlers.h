#ifndef CRADLE_RPCLIB_SERVER_HANDLERS_H
#define CRADLE_RPCLIB_SERVER_HANDLERS_H

#include <string>

#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/typing/service/core.h>

namespace cradle {

struct rpclib_handler_context
{
    service_core& service;
    spdlog::logger& logger;
};

cppcoro::task<blob>
handle_resolve_sync(
    rpclib_handler_context& hctx,
    std::string const& domain_name,
    std::string const& seri_req);

cppcoro::task<void>
handle_mock_http(rpclib_handler_context& hctx, std::string const& body);

} // namespace cradle

#endif
