#include <cradle/inner/service/config.h>
#include <cradle/rpclib/client/contained_proxy_pool.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/common/config.h>

namespace cradle {

std::unique_ptr<rpclib_client>
contained_proxy_pool::alloc_proxy(
    service_config const& config, std::shared_ptr<spdlog::logger> logger)
{
    std::scoped_lock lock{mutex_};
    std::unique_ptr<rpclib_client> proxy;
    if (!available_proxies_.empty())
    {
        proxy = std::move(available_proxies_.front());
        available_proxies_.pop_front();
        logger->info("reusing proxy with port {}", proxy->get_port());
    }
    else
    {
        proxy = std::make_unique<rpclib_client>(config, &port_owner_, logger);
        logger->info("created new proxy with port {}", proxy->get_port());
    }
    return proxy;
}

void
contained_proxy_pool::free_proxy(
    std::unique_ptr<rpclib_client> proxy, bool succeeded)
{
    std::scoped_lock lock{mutex_};
    if (succeeded)
    {
        available_proxies_.push_back(std::move(proxy));
    }
    // else call proxy's destructor
}

} // namespace cradle
