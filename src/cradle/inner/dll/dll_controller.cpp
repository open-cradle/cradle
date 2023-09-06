#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/resolve/meta_catalog.h>

namespace cradle {

dll_controller::dll_controller(std::string path, std::string name)
    : path_{std::move(path)}, name_{std::move(name)}
{
}

void
dll_controller::load()
{
    // TODO Consider rtld_lazy if the library is opened only for getting the
    // uuid's, as this might be significantly faster than rtld_now.
    auto mode{
        boost::dll::load_mode::rtld_now | boost::dll::load_mode::rtld_local
        | boost::dll::load_mode::rtld_deepbind};
    lib_ = std::make_unique<boost::dll::shared_library>(path_, mode);

    typedef void init_func_t();
    std::string const init_func_name{"CRADLE_init"};
    typedef seri_catalog* get_catalog_func_t();
    std::string const get_catalog_func_name{"CRADLE_get_catalog"};

    auto init_func = lib_->get<init_func_t>(init_func_name);
    init_func();

    auto get_catalog_func
        = lib_->get<get_catalog_func_t>(get_catalog_func_name);
    catalog_ = get_catalog_func();
    meta_catalog::instance().add_catalog(*catalog_);
}

void
dll_controller::unload()
{
    for (auto const& uuid_str : catalog_->get_all_uuid_strs())
    {
        cereal_functions_registry::instance().remove_entry_if_exists(uuid_str);
    }
    meta_catalog::instance().remove_catalog(*catalog_);
    catalog_ = nullptr;
    lib_.reset();
}

} // namespace cradle
