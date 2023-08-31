#ifndef CRADLE_INNER_RESOLVE_META_CATALOG_IMPL_H
#define CRADLE_INNER_RESOLVE_META_CATALOG_IMPL_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/dll.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_result.h>

namespace cradle {

class seri_catalog_controller
{
 public:
    // catalog should be a static (either in the main program, or a DLL)
    seri_catalog_controller(seri_catalog& catalog) : catalog_{catalog}
    {
    }

    virtual ~seri_catalog_controller() = default;

    seri_catalog&
    get_catalog()
    {
        return catalog_;
    }

 private:
    seri_catalog& catalog_;
};

class static_catalog_controller : public seri_catalog_controller
{
 public:
    static_catalog_controller(seri_catalog& catalog)
        : seri_catalog_controller(catalog)
    {
    }
};

class dll_catalog_controller : public seri_catalog_controller
{
 public:
    dll_catalog_controller(
        seri_catalog& catalog,
        std::string dll_path,
        std::unique_ptr<boost::dll::shared_library> lib)
        : seri_catalog_controller(catalog),
          dll_path_{std::move(dll_path)},
          lib_{std::move(lib)}
    {
    }

 private:
    std::string dll_path_;
    std::unique_ptr<boost::dll::shared_library> lib_;
};

class meta_catalog_impl
{
 public:
    void
    add_static_catalog(seri_catalog& catalog);

    void
    add_dll_catalog(
        seri_catalog& catalog,
        std::string dll_path,
        std::unique_ptr<boost::dll::shared_library> lib);

    cppcoro::task<serialized_result>
    resolve(local_context_intf& ctx, std::string seri_req);

 private:
    std::vector<std::unique_ptr<seri_catalog_controller>> controllers_;
    std::unordered_map<std::string, seri_catalog_controller*>
        controllers_by_uuid_;

    void
    add_controller(std::unique_ptr<seri_catalog_controller> controller);

    std::string
    find_uuid_str(std::string const& seri_req);

    std::shared_ptr<seri_resolver_intf>
    find_resolver(std::string const& uuid_str);
};

} // namespace cradle

#endif
