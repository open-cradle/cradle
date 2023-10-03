#ifndef CRADLE_INNER_REQUEST_DOMAIN_H
#define CRADLE_INNER_REQUEST_DOMAIN_H

#include <memory>
#include <string>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/config.h>

namespace cradle {

// A remote "resolve request" command includes a domain name, which specifies
// the context class that should be used for resolving the request.
// A domain may also contain a catalog of seri resolvers.
class domain
{
 public:
    virtual ~domain() = default;

    virtual std::string
    name() const
        = 0;

    // TODO can these be unique_ptr?
    virtual std::shared_ptr<sync_context_intf>
    make_local_sync_context(service_config const& config) const = 0;

    virtual std::shared_ptr<async_context_intf>
    make_local_async_context(service_config const& config) const = 0;
};

} // namespace cradle

#endif
