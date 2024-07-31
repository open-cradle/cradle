#ifndef CRADLE_INNER_RESOLVE_SERI_CATALOG_H
#define CRADLE_INNER_RESOLVE_SERI_CATALOG_H

#include <memory>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/types.h>
#include <cradle/inner/resolve/seri_resolver.h>

namespace cradle {

class seri_registry;

/**
 * Catalog of resolvers that can locally resolve a serialized request
 *
 * An application has a number of catalogs that are typically created
 * during initialization. In addition, catalogs can be added by loading
 * DLLs; a DLL has exactly one catalog.
 *
 * A request is characterized by its uuid (as a string).
 * A request is resolved to a serialized response.
 * The catalog maps uuid's to type-erased seri_resolver_impl objects, so
 * contains references to seri_resolver_intf's. The actual map is kept in the
 * seri_registry singleton, not in the seri_catalog object itself.
 *
 * An object of this class is identified by a unique catalog_id value.
 * Loading, unloading and reloading a DLL gives two different catalogs,
 * with different catalog_id values.
 *
 * Production code typically creates selfreg_seri_catalog objects; directly
 * creating seri_catalog objects seems useful in unit tests code only.
 */
class seri_catalog
{
 public:
    // ~seri_catalog() needs the registry object to still be there.
    seri_catalog(std::shared_ptr<seri_registry> registry);

    virtual ~seri_catalog();

    catalog_id
    get_cat_id() const
    {
        return cat_id_;
    }

    /**
     * Registers a resolver for a uuid, from a template/sample request object.
     *
     * The resolver can resolve serialized requests that are
     * similar to the template one; different arguments are allowed, but
     * otherwise the request should be identical to the template.
     */
    // TODO register_resolver() also needed for (de_)serializing a request;
    // rename? register_uuid() for (de-)serializing and resolving
    template<Request Req>
    void
    register_resolver(Req const& req)
        requires(!Req::is_proxy)
    {
        req.register_uuid(
            *registry_, cat_id_, std::make_shared<seri_resolver_impl<Req>>());
    }

    // Needed to deserialize a (stored) proxy request.
    // No attempt should be made to call its resolver, because it doesn't have
    // any.
    template<Request Req>
    void
    register_proxy(Req const& req)
        requires(Req::is_proxy)
    {
        req.register_uuid(*registry_, cat_id_);
    }

 protected:
    // Unregisters all resolvers that were successfully registered
    // Not thread-safe
    void
    unregister_all() noexcept;

 private:
    std::shared_ptr<seri_registry> registry_;
    catalog_id cat_id_;
};

/*
 * A self-registering seri_catalog variant: the object registers all resolvers,
 * rather than the client performing the register_resolver() calls.
 * The catalog's seri resolvers become available in the constructor,
 * and stay available until the object is destroyed.
 * If a register_resolver() call from the constructor throws, the seri_catalog
 * destructor will unregister any preceding successfully registered entries.
 */
class selfreg_seri_catalog : public seri_catalog
{
 public:
    selfreg_seri_catalog(std::shared_ptr<seri_registry> registry);

 protected:
    // register_resolver() to be called from a derived class's constructor only
    using seri_catalog::register_resolver;
};

} // namespace cradle

#endif
