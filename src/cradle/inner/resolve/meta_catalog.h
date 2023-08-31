#ifndef CRADLE_INNER_RESOLVE_META_CATALOG_H
#define CRADLE_INNER_RESOLVE_META_CATALOG_H

#include <memory>
#include <string>

#include <boost/dll.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_result.h>

namespace cradle {

class meta_catalog_impl;

// Singleton catalog of all global seri_catalog objects in the system.
// These objects originate from the main program or from a DLL; in either case,
// they must be static's.
class meta_catalog
{
 public:
    static meta_catalog&
    instance();

    meta_catalog();

    // catalog should be a static object (in the main program).
    void
    add_static_catalog(seri_catalog& catalog);

    // catalog should be a static object (in the DLL).
    // When lib's destructor is called, the library will/may be unloaded, and
    // the catalog reference becomes dangling. The meta_catalog must somehow
    // handle this (e.g. by never letting go of the lib unique_ptr).
    void
    add_dll_catalog(
        seri_catalog& catalog,
        std::string dll_path,
        std::unique_ptr<boost::dll::shared_library> lib);

    /**
     * Locally resolves a serialized request to a serialized response.
     *
     * The request is characterized by a uuid encoded in seri_req.
     * Throws uuid_error if the uuid does not appear in any seri catalog.
     */
    cppcoro::task<serialized_result>
    resolve(local_context_intf& ctx, std::string seri_req);

 private:
    std::unique_ptr<meta_catalog_impl> impl_;
};

} // namespace cradle

#endif
