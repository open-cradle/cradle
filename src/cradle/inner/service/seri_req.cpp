#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/seri_catalog.h>
#include <cradle/inner/service/seri_req.h>

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

// TODO need to refactor like resolve_request()?
cppcoro::task<serialized_result>
resolve_serialized_request(context_intf& ctx, std::string seri_req)
{
    if (remote_context_intf* rem_ctx = to_remote_ptr(ctx))
    {
        return resolve_serialized_remote(*rem_ctx, std::move(seri_req));
    }
    else
    {
        local_context_intf& loc_ctx = to_local_ref(ctx);
        return resolve_serialized_local(loc_ctx, std::move(seri_req));
    }
}

} // namespace cradle
