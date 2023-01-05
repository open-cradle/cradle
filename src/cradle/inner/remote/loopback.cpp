#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/remote/loopback_impl.h>
#include <cradle/inner/service/seri_req.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

loopback_service::loopback_service()
{
    if (!logger_)
    {
        logger_ = create_logger("loopback");
    }
}

std::string
loopback_service::name() const
{
    return "loopback";
}

cppcoro::task<blob>
loopback_service::resolve_request(
    remote_context_intf& ctx, std::string seri_req)
{
    logger_->debug("request: {}", seri_req);
    auto local_ctx{ctx.local_clone()};
    blob result
        = co_await resolve_serialized_request(*local_ctx, std::move(seri_req));
    logger_->debug("response {}", result);
    co_return result;
}

void
register_loopback_service()
{
    auto proxy{std::make_shared<loopback_service>()};
    register_proxy(proxy);
}

} // namespace cradle
