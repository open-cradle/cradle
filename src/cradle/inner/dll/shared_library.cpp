#include <memory>

#include <boost/dll.hpp>
#include <fmt/format.h>

#include <cradle/inner/dll/shared_library.h>
#include <cradle/inner/resolve/meta_catalog.h>
#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

void
load_shared_library(std::string const& dir_path, std::string const& dll_name)
{
#ifdef _WIN32
    std::string dll_path{fmt::format("{}/{}.dll", dir_path, dll_name)};
#else
    std::string dll_path{fmt::format("{}/lib{}.so", dir_path, dll_name)};
#endif
    // TODO Consider rtld_lazy if the library is opened only for getting the
    // uuid's, as this might be significantly faster than rtld_now.
    auto mode{
        boost::dll::load_mode::rtld_now | boost::dll::load_mode::rtld_local
        | boost::dll::load_mode::rtld_deepbind};
    auto lib{make_unique<boost::dll::shared_library>(dll_path, mode)};

    typedef void init_func_t();
    std::string const init_func_name{"CRADLE_init"};
    typedef seri_catalog* get_catalog_func_t();
    std::string const get_catalog_func_name{"CRADLE_get_catalog"};

    auto init_func = lib->get<init_func_t>(init_func_name);
    init_func();

    auto get_catalog_func
        = lib->get<get_catalog_func_t>(get_catalog_func_name);
    auto* catalog = get_catalog_func();
    meta_catalog::instance().add_dll_catalog(
        *catalog, std::move(dll_path), std::move(lib));
}

} // namespace cradle
