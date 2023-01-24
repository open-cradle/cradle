#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/service/remote.h>

namespace cradle {

cppcoro::task<serialized_result>
resolve_remote(remote_context_intf& ctx, std::string const& seri_req)
{
    auto proxy = find_proxy(ctx.proxy_name());
    return proxy->resolve_request(ctx, seri_req);
}

} // namespace cradle
