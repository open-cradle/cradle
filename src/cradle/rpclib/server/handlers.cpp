#include <cassert>
#include <stdexcept>

#include <rpc/this_handler.h>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/requests/domain.h>
#include <cradle/inner/service/seri_req.h>
#include <cradle/rpclib/server/handlers.h>
#include <cradle/typing/service/core.h>

namespace cradle {

rpclib_handler_context::rpclib_handler_context(
    service_core& service, spdlog::logger& logger)
    : service_{service}, logger_{logger}
{
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

cppcoro::task<rpclib_response>
handle_resolve_sync(
    rpclib_handler_context& hctx,
    std::string const& domain_name,
    std::string const& seri_req)
{
    blob result;
    auto& service{hctx.service()};
    try
    {
        auto& logger{hctx.logger()};
        logger.info("resolve_sync {}: {}", domain_name, seri_req);
        auto dom = find_domain(domain_name);
        auto ctx{dom->make_local_context(service)};
        auto seri_result = co_await resolve_serialized_request(*ctx, seri_req);
        // TODO try to get rid of .value()
        result = seri_result.value();
        logger.info("result {}", result);
    }
    catch (std::exception& e)
    {
        handle_exception(hctx, e);
    }
    // TODO if the result references blob files, then create a response_id
    // uniquely identifying the set of those files
    static uint32_t response_id = 0;
    response_id += 1;
    co_return std::make_tuple(response_id, std::move(result));
}

cppcoro::task<void>
handle_ack_response(rpclib_handler_context& hctx, int response_id)
{
    auto& logger{hctx.logger()};
    logger.info("ack_response {}", response_id);
    // TODO release the temporary lock on the blob files referenced in
    // response #response_id
    co_return;
}

cppcoro::task<void>
handle_mock_http(rpclib_handler_context& hctx, std::string const& body)
{
    auto& session = enable_http_mocking(hctx.service());
    session.set_canned_response(make_http_200_response(body));
    co_return;
}

} // namespace cradle
