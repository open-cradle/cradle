#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_req.h>

namespace cradle {

cppcoro::task<serialized_result>
resolve_serialized_remote(remote_context_intf& ctx, std::string seri_req)
{
    co_return resolve_remote(ctx, std::move(seri_req));
}

cppcoro::task<serialized_result>
resolve_serialized_local(local_context_intf& ctx, std::string seri_req)
{
    return seri_catalog::instance().resolve(ctx, std::move(seri_req));
}

// Currently only called from websocket/server.cpp
cppcoro::task<serialized_result>
resolve_serialized_request(context_intf& ctx, std::string seri_req)
{
    if (auto* rem_ctx = cast_ctx_to_ptr<remote_context_intf>(ctx))
    {
        return resolve_serialized_remote(*rem_ctx, std::move(seri_req));
    }
    else
    {
        auto& loc_ctx = cast_ctx_to_ref<local_context_intf>(ctx);
        return resolve_serialized_local(loc_ctx, std::move(seri_req));
    }
}

} // namespace cradle
