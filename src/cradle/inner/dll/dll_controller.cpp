#include <fmt/format.h>

#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/dll/dll_exceptions.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

dll_controller::dll_controller(
    dll_trash& trash, std::string path, std::string name)
    : trash_{trash},
      path_{std::move(path)},
      name_{std::move(name)},
      logger_{ensure_logger("dll")}
{
    load();
}

dll_controller::~dll_controller()
{
    try
    {
        unload();
    }
    catch (...)
    {
    }
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
    lib_ = new boost::dll::shared_library(path_, mode);

    typedef selfreg_seri_catalog* create_catalog_func_t();
    std::string const create_catalog_func_name{"CRADLE_create_seri_catalog"};

    auto create_catalog_func
        = lib_->get<create_catalog_func_t>(create_catalog_func_name);
    catalog_.reset(create_catalog_func());
    if (!catalog_)
    {
        throw dll_load_error("create_catalog_func() failed");
    }
    auto cat_id_value{catalog_->get_cat_id().value()};
    logger_->info("load done for {} -> cat_id {}", name_, cat_id_value);
    catalog_->register_all();
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
        catalog_.reset();
    }
    else
    {
        logger_->warn("unload {} - no catalog", name_);
    }
    if (lib_)
    {
        trash_.add(lib_);
        logger_->info("Now have {} inactive DLLs", trash_.size());
    }
    logger_->info("unload done for {}", name_);
}

} // namespace cradle
