#include <cradle/inner/remote/proxy.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/registry.h>

namespace cradle {

std::shared_ptr<rpclib_client>
register_rpclib_client(service_config const& config)
{
    auto proxy{std::make_shared<rpclib_client>(config)};
    register_proxy(proxy);
    return proxy;
}

} // namespace cradle
