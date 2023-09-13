#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/dll/dll_singleton.h>
#include <cradle/inner/dll/shared_library.h>

namespace cradle {

void
load_shared_library(std::string const& dir_path, std::string const& dll_name)
{
    auto logger{spdlog::get("cradle")};
#ifdef _WIN32
    std::string dll_path{fmt::format("{}/{}.dll", dir_path, dll_name)};
#else
    std::string dll_path{fmt::format("{}/lib{}.so", dir_path, dll_name)};
#endif
    auto controller{
        std::make_unique<dll_controller>(std::move(dll_path), dll_name)};
    try
    {
        controller->load();
    }
    catch (std::exception& e)
    {
        // e's actual type may be defined in the DLL; if so, its code will
        // disappear in controller's destructor.
        // TODO what if any of the following lines throws?
        std::string what{e.what()};
        logger->error(
            "load_shared_library({}, {}) failed: {}",
            dir_path,
            dll_name,
            what);
        // TODO unload() may also fail
        controller->unload();
        throw dll_load_error{what};
    }
    // TODO what if the following fails?
    dll_singleton::instance().add(std::move(controller));
}

void
unload_shared_library(std::string const& dll_name)
{
    for (auto& controller : dll_singleton::instance().remove(dll_name))
    {
        controller->unload();
    }
}

} // namespace cradle
