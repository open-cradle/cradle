#include <fmt/format.h>

#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

dll_controller::dll_controller(std::string path, std::string name)
    : path_{std::move(path)},
      name_{std::move(name)},
      logger_{ensure_logger("dll")}
{
}

dll_controller::~dll_controller()
{
    // Calling this destructor unloads the DLL, crashing the application if
    // references to the DLL code still exist, and we currently do not try
    // to delete all of them.
    logger_->error("~dll_controller() must not be called");
}

void
dll_controller::load()
{
    logger_->info("load {} from {}", name_, path_);
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

    // TODO if init_func fails, data specific for this DLL may have been stored
    // in the singletons, and would/should be accessible through catalog_ but
    // we don't have retrieved that yet

    auto get_catalog_func
        = lib_->get<get_catalog_func_t>(get_catalog_func_name);
    // TODO the only thing done with the catalog is to retrieve its cat_id.
    // Consider changing the interface to the DLL.
    catalog_ = get_catalog_func();
    auto cat_id_value{catalog_->get_cat_id().value()};
    logger_->info("load done for {} -> cat_id {}", name_, cat_id_value);
    seri_registry::instance().log_all_entries(
        fmt::format("after load cat_id {}", cat_id_value));
}

void
dll_controller::unload()
{
    if (catalog_)
    {
        logger_->info(
            "unload {} (cat_id {})", name_, catalog_->get_cat_id().value());
        catalog_->unregister_all();
        catalog_ = nullptr;
    }
    else
    {
        logger_->warn("unload {} - no catalog", name_);
    }
    // The following would unload the DLL, causing a ~seri_catalog() call
    // unregistering the catalog's resolvers if that wasn't done yet.
    // As we (currently) have no guarantee that no other references to the
    // DLL's code remain, we do not really unload the DLL.
    // lib_.reset();
    logger_->info("unload done for {}", name_);
}

} // namespace cradle
