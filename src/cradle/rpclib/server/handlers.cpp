#include <stdexcept>

#include <rpc/this_handler.h>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/requests/domain.h>
#include <cradle/inner/service/seri_req.h>
#include <cradle/rpclib/server/handlers.h>
#include <cradle/typing/service/core.h>

namespace cradle {

// [[noreturn]]
// Throws something that is handled inside the rpclib library
static void
handle_exception(rpclib_handler_context& hctx, std::exception& e)
{
    hctx.logger.error("caught {}", e.what());
    rpc::this_handler().respond_error(e.what());
}

cppcoro::task<blob>
handle_resolve_sync(
    rpclib_handler_context& hctx,
    std::string const& domain_name,
    std::string const& seri_req)
{
    blob result;
    try
    {
        auto& logger{hctx.logger};
        logger.info("resolve_sync {}: {}", domain_name, seri_req);
        auto dom = find_domain(domain_name);
        auto ctx{dom->make_local_context(hctx.service)};
        result = co_await resolve_serialized_request(*ctx, seri_req);
        logger.info("result {}", result);
    }
    catch (std::exception& e)
    {
        handle_exception(hctx, e);
    }
    co_return result;
}

cppcoro::task<void>
handle_mock_http(rpclib_handler_context& hctx, std::string const& body)
{
    auto& session = enable_http_mocking(hctx.service);
    session.set_canned_response(make_http_200_response(body));
    co_return;
}

} // namespace cradle
