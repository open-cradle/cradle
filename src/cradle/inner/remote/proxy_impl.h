#ifndef CRADLE_INNER_REMOTE_PROXY_IMPL_H
#define CRADLE_INNER_REMOTE_PROXY_IMPL_H

#include <memory>
#include <string>
#include <unordered_map>

#include <cradle/inner/remote/proxy.h>

namespace cradle {

class remote_proxy_registry
{
 public:
    static remote_proxy_registry&
    instance();

    void
    register_proxy(std::shared_ptr<remote_proxy> proxy);

    std::shared_ptr<remote_proxy>
    find_proxy(std::string const& name);

 private:
    std::unordered_map<std::string, std::shared_ptr<remote_proxy>> proxies_;
};

} // namespace cradle

#endif
