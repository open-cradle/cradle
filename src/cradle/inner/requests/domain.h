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

    // Creates a sync context that can be used for any number of local request
    // resolutions (unlike async contexts, sync ones need no preparation).
    virtual std::shared_ptr<local_sync_context_intf>
    make_local_sync_context(service_config const& config) const = 0;

    // Creates an async context that can be used for exactly one local request
    // resolution (and has been prepared for that one resolution).
    virtual std::shared_ptr<root_local_async_context_intf>
    make_local_async_context(service_config const& config) const = 0;
};

} // namespace cradle

#endif
