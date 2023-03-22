#include <stdexcept>

#include <fmt/core.h>

#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/remote/proxy_impl.h>

namespace cradle {

remote_proxy_registry&
remote_proxy_registry::instance()
{
    static remote_proxy_registry the_instance;
    return the_instance;
}

void
remote_proxy_registry::register_proxy(std::shared_ptr<remote_proxy> proxy)
{
    proxies_[proxy->name()] = proxy;
}

std::shared_ptr<remote_proxy>
remote_proxy_registry::find_proxy(std::string const& name)
{
    auto it = proxies_.find(name);
    if (it == proxies_.end())
    {
        return std::shared_ptr<remote_proxy>{};
    }
    return it->second;
}

void
register_proxy(std::shared_ptr<remote_proxy> proxy)
{
    remote_proxy_registry::instance().register_proxy(proxy);
}

std::shared_ptr<remote_proxy>
find_proxy(std::string const& name)
{
    auto proxy = remote_proxy_registry::instance().find_proxy(name);
    if (!proxy)
    {
        auto what = fmt::format("Proxy {} not registered", name);
        throw std::logic_error(what);
    }
    return proxy;
}

} // namespace cradle
