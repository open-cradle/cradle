#ifndef CRADLE_INNER_RESOLVE_SERI_CATALOG_H
#define CRADLE_INNER_RESOLVE_SERI_CATALOG_H

#include <memory>
#include <string>
#include <unordered_map>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/seri_resolver.h>

namespace cradle {

/**
 * Catalog of resolvers that can locally resolve a serialized request

 * An application can have a number of catalogs that are typically created
 * during initialization. In addition, catalogs can be added by loading
 * DLLs; a DLL has exactly one catalog.
 *
 * A request is characterized by its uuid (as a string).
 * A request is resolved to a serialized response.
 * The catalog maps uuid's to type-erased seri_resolver_impl objects, so
 * contains references to seri_resolver_intf's.
 *
 * An object of this class is assumed to be created and populated once, at
 * initialization time, by a single thread, and be immutable thereafter.
 */
class seri_catalog
{
 public:
    /**
     * Registers a resolver for a uuid, from a template/sample request object.
     *
     * The resolver will be able to resolve serialized requests that are
     * similar to the template one; different arguments are allowed, but
     * otherwise the request should be identical to the template.
     */
    template<Request Req>
    void
    register_resolver(Req const& req)
    {
        // TODO add Request::is_proxy and throw here if true
        auto uuid_str{req.get_uuid().str()};
        map_[uuid_str] = std::make_shared<seri_resolver_impl<Req>>();
    }

    // Returns all uuids registered in this catalog.
    std::vector<std::string>
    get_all_uuid_strs() const;

    // Gets the resolver for the given uuid, which must be one of the
    // get_all_uuid_strs() return values.
    std::shared_ptr<seri_resolver_intf>
    get_resolver(std::string const& uuid_str)
    {
        return map_.at(uuid_str);
    }

 private:
    std::unordered_map<std::string, std::shared_ptr<seri_resolver_intf>> map_;
};

} // namespace cradle

#endif
