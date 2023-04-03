#ifndef CRADLE_INNER_SERVICE_SERI_CATALOG_H
#define CRADLE_INNER_SERVICE_SERI_CATALOG_H

#include <memory>
#include <string>
#include <unordered_map>

#include <cppcoro/task.hpp>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/seri_resolver.h>
#include <cradle/inner/service/seri_result.h>

namespace cradle {

/**
 * Catalog of resolvers that can locally resolve a serialized request
 *
 * Singleton.
 * A request is characterized by its uuid (as a string).
 * A request is resolved to a serialized response.
 * The catalog maps uuid's to type-erased seri_resolver_impl objects, so
 * contains references to seri_resolver_intf's.
 */
class seri_catalog
{
 public:
    /**
     * Returns the singleton
     */
    static seri_catalog&
    instance();

    /**
     * Registers a resolver for a uuid
     */
    template<Context Ctx, Request Req>
    void
    register_resolver(std::string const& uuid_str)
    {
        map_[uuid_str] = std::make_shared<seri_resolver_impl<Ctx, Req>>();
    }

    /**
     * Locally resolves a serialized request appearing in this catalog, to
     * a serialized response
     *
     * The request is characterized by a uuid encoded in seri_req.
     * Throws uuid_error if the uuid does not appear in the catalog.
     */
    cppcoro::task<serialized_result>
    resolve(context_intf& ctx, std::string seri_req);

 private:
    std::unordered_map<std::string, std::shared_ptr<seri_resolver_intf>> map_;

    std::string
    find_uuid_str(std::string const& seri_req);

    std::shared_ptr<seri_resolver_intf>
    find_resolver(std::string const& uuid_str);
};

/**
 * Registers a resolver from a template/sample request object.
 *
 * The resolver will be able to resolve serialized requests that are similar
 * to the template one; different arguments are allowed, but otherwise the
 * request should be identical to the template.
 *
 * Context at resolution time should equal Ctx.
 */
template<Context Ctx, Request Req>
void
register_seri_resolver(Req const& req)
{
    seri_catalog::instance().register_resolver<Ctx, Req>(req.get_uuid().str());
}

} // namespace cradle

#endif
