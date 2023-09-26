#ifndef CRADLE_INNER_RESOLVE_SERI_CATALOG_H
#define CRADLE_INNER_RESOLVE_SERI_CATALOG_H

#include <memory>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/types.h>
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
 *
 * An object of this class is identified by a unique catalog_id value.
 * Loading, unloading and reloading a DLL gives two different catalogs,
 * with different catalog_id values.
 */
class seri_catalog
{
 public:
    ~seri_catalog();

    catalog_id
    get_cat_id()
    {
        return cat_id_;
    }

    /**
     * Registers a resolver for a uuid, from a template/sample request object.
     *
     * The resolver will be able to resolve serialized requests that are
     * similar to the template one; different arguments are allowed, but
     * otherwise the request should be identical to the template.
     */
    // TODO register_resolver() also needed for (de_)serializing a request;
    // rename? register_uuid() for (de-)serializing and resolving
    template<Request Req>
    void
    register_resolver(Req const& req)
    {
        // Not thread-safe (not needed: see above).
        // TODO add Request::is_proxy and throw here if true
        req.register_uuid(
            cat_id_, std::make_shared<seri_resolver_impl<Req>>());
    }

    void
    unregister_all() noexcept;

 private:
    catalog_id cat_id_;
};

} // namespace cradle

#endif
