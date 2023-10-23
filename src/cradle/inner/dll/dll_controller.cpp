#include <fmt/format.h>

#include <cradle/inner/dll/dll_capabilities.h>
#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/dll/dll_exceptions.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

dll_controller::dll_controller(
    inner_resources& resources,
    dll_trash& trash,
    spdlog::logger& logger,
    std::string path,
    std::string name)
    : resources_{resources},
      trash_{trash},
      logger_{logger},
      path_{std::move(path)},
      name_{std::move(name)}
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
    logger_.info("load {} from {}", name_, path_);
    // TODO Consider rtld_lazy if the library is opened only for getting the
    // uuid's, as this might be significantly faster than rtld_now.
    auto mode{
        boost::dll::load_mode::rtld_now | boost::dll::load_mode::rtld_local
        | boost::dll::load_mode::rtld_deepbind};
    lib_ = new boost::dll::shared_library(path_, mode);

    using get_capabilities_func_t = dll_capabilities const*();
    std::string const get_capabilities_func_name{"CRADLE_get_capabilities"};

    get_capabilities_func_t* get_caps_func{nullptr};
    try
    {
        // Throws if the DLL does not export the mandatory symbol.
        get_caps_func
            = lib_->get<get_capabilities_func_t>(get_capabilities_func_name);
    }
    catch (std::exception const&)
    {
        throw dll_load_error{fmt::format(
            "Error loading {}: DLL does not export {}",
            path_,
            get_capabilities_func_name)};
    }
    // Should the DLL export a symbol with the correct name but the wrong type,
    // then the application will most likely crash.
    auto const* caps = get_caps_func();
    if (!caps)
    {
        throw dll_load_error{fmt::format(
            "Error loading {}: get_caps_func() returned nullptr", path_)};
    }
    create_seri_catalog(*caps);
    logger_.info("load done for {}", name_);
}

void
dll_controller::create_seri_catalog(dll_capabilities const& caps)
{
    auto* create_catalog_func{caps.create_seri_catalog};
    if (!create_catalog_func)
    {
        return;
    }
    auto registry{resources_.get_seri_registry()};
    catalog_ = create_catalog_func(registry);
    if (!catalog_)
    {
        throw dll_load_error{fmt::format(
            "Error loading {}: create_catalog_func() returned nullptr",
            path_)};
    }
    auto cat_id_value{catalog_->get_cat_id().value()};
    logger_.info("loaded catalog #{}", cat_id_value);
    registry->log_all_entries(
        fmt::format("after load cat_id {}", cat_id_value));
}

// Called from the destructor.
// Can throw only on out-of-memory conditions, or if logging fails.
void
dll_controller::unload()
{
    if (catalog_)
    {
        logger_.info(
            "unload {} (cat_id {})", name_, catalog_->get_cat_id().value());
    }
    trash_.add(lib_);
    logger_.info("Now have {} inactive DLLs", trash_.size());
    logger_.info("unload done for {}", name_);
}

} // namespace cradle
