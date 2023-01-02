#ifndef CRADLE_INNER_REMOTE_PROXY_H
#define CRADLE_INNER_REMOTE_PROXY_H

#include <memory>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/requests/generic.h>

namespace cradle {

class remote_proxy
{
 public:
    virtual ~remote_proxy() = default;

    virtual std::string
    name() const = 0;

    // Returns serialized response
    // Throws on error
    // TODO replace blob with some kind of shared string?
    virtual cppcoro::task<blob>
    resolve_request(remote_context_intf& ctx, std::string seri_req) = 0;
};

void
register_proxy(std::shared_ptr<remote_proxy> proxy);

std::shared_ptr<remote_proxy>
find_proxy(std::string const& name);

} // namespace cradle

#endif
