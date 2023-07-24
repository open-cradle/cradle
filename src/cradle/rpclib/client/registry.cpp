#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/service/resources.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/registry.h>

namespace cradle {

rpclib_client&
register_rpclib_client(
    service_config const& config, inner_resources& resources)
{
    auto proxy{std::make_unique<rpclib_client>(config)};
    auto& res{*proxy};
    resources.register_proxy(std::move(proxy));
    return res;
}

} // namespace cradle
