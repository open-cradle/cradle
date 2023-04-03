#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/remote/loopback_impl.h>
#include <cradle/inner/requests/domain.h>
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

spdlog::logger&
loopback_service::get_logger()
{
    return *logger_;
}

cppcoro::static_thread_pool&
loopback_service::get_coro_thread_pool()
{
    return coro_thread_pool_;
}

cppcoro::task<serialized_result>
loopback_service::resolve_sync(
    remote_context_intf& ctx, std::string domain_name, std::string seri_req)
{
    logger_->debug("resolve_sync({}): request {}", domain_name, seri_req);
    auto local_ctx{ctx.local_clone()};
    auto result
        = co_await resolve_serialized_request(*local_ctx, std::move(seri_req));
    logger_->debug("response {}", result.value());
    co_return result;
}

cppcoro::task<async_id>
loopback_service::submit_async(std::string domain_name, std::string seri_req)
{
    throw not_implemented_error("loopback_service::submit_async()");
}

cppcoro::task<remote_context_spec_list>
loopback_service::get_sub_contexts(async_id aid)
{
    throw not_implemented_error("loopback_service::get_sub_contexts()");
}

cppcoro::task<async_status>
loopback_service::get_async_status(async_id aid)
{
    throw not_implemented_error("loopback_service::get_async_status()");
}

cppcoro::task<serialized_result>
loopback_service::get_async_response(async_id root_aid)
{
    throw not_implemented_error("loopback_service::get_async_response()");
}

cppcoro::task<void>
loopback_service::request_cancellation(async_id aid)
{
    throw not_implemented_error("loopback_service::request_cancellation()");
}

cppcoro::task<void>
loopback_service::finish_async(async_id root_aid)
{
    throw not_implemented_error("loopback_service::finish_async()");
}

void
register_loopback_service()
{
    auto proxy{std::make_shared<loopback_service>()};
    register_proxy(proxy);
}

} // namespace cradle
