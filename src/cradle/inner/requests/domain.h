#ifndef CRADLE_INNER_REQUEST_DOMAIN_H
#define CRADLE_INNER_REQUEST_DOMAIN_H

#include <memory>
#include <string>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

// A remote "resolve request" command includes a domain name, which specifies
// the context class that should be used for resolving the request.
class domain
{
 public:
    virtual ~domain() = default;

    virtual void
    initialize()
        = 0;

    virtual std::string
    name() const
        = 0;

    // TODO pass in the request's required context types, so that the returned
    // contructed context object can be guaranteed to implement them all
    // (the factory function should throw if it cannot create such an object).
    virtual std::shared_ptr<sync_context_intf>
    make_sync_context(
        inner_resources& resources, bool remotely, std::string proxy_name)
        = 0;

    virtual std::shared_ptr<async_context_intf>
    make_async_context(
        inner_resources& resources, bool remotely, std::string proxy_name)
        = 0;
};

void
register_domain(std::shared_ptr<domain> const& dom);

// Throws if not found
std::shared_ptr<domain>
find_domain(std::string const& name);

} // namespace cradle

#endif
