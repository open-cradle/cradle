#include <cradle/inner/service/seri_catalog.h>
#include <cradle/inner/service/seri_req.h>

namespace cradle {

static cppcoro::task<serialized_result>
resolve_remote_coro(remote_context_intf& ctx, std::string seri_req)
{
    co_return resolve_remote(ctx, std::move(seri_req));
}

// TODO need to refactor like resolve_request()?
cppcoro::task<serialized_result>
resolve_serialized_request(context_intf& ctx, std::string seri_req)
{
    if (remote_context_intf* rem_ctx = to_remote_context_intf(ctx))
    {
        return resolve_remote_coro(*rem_ctx, std::move(seri_req));
    }
    else
    {
        return seri_catalog::instance().resolve(ctx, std::move(seri_req));
    }
}

} // namespace cradle
