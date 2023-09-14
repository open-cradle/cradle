#ifndef CRADLE_INNER_RESOLVE_META_CATALOG_H
#define CRADLE_INNER_RESOLVE_META_CATALOG_H

#include <mutex>
#include <string>
#include <unordered_map>

#include <cppcoro/task.hpp>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_result.h>

namespace cradle {

// Singleton catalog of all seri_catalog objects in the system, and the
// capability to resolve serialized requests through any of them.
// A seri_catalog object originates from the main program or from a DLL.
// Ownership lies elsewhere; the objects could e.g. be singletons themselves.
class meta_catalog
{
 public:
    static meta_catalog&
    instance();

    void
    add_catalog(seri_catalog& catalog);

    void
    remove_catalog(seri_catalog& catalog);

    /**
     * Locally resolves a serialized request to a serialized response.
     *
     * The request is characterized by a uuid encoded in seri_req.
     * Throws uuid_error if the uuid does not appear in any seri catalog.
     */
    // TODO replace with find_resolver()
    // and add find_function_entry()
    cppcoro::task<serialized_result>
    resolve(local_context_intf& ctx, std::string seri_req);

 private:
    std::mutex mutex_;
    // TODO should be multimap?
    std::unordered_map<std::string, seri_catalog*> catalogs_map_;

    std::string
    extract_uuid_str(std::string const& seri_req);

    std::shared_ptr<seri_resolver_intf>
    find_resolver(std::string const& uuid_str);
};

} // namespace cradle

#endif
