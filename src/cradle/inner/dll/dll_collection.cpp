#include <fmt/format.h>

#include <cradle/inner/dll/dll_collection.h>
#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/dll/dll_exceptions.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

dll_collection::dll_collection(inner_resources& resources)
    : resources_{resources}, logger_{ensure_logger("dll")}
{
}

dll_collection::~dll_collection() = default;

void
dll_collection::load(std::string const& dir_path, std::string const& dll_name)
{
    std::scoped_lock lock{mutex_};
    if (controllers_.contains(dll_name))
    {
        logger_->warn("DLL {} already loaded", dll_name);
        return;
    }
    std::unique_ptr<dll_controller> controller;
    try
    {
        controller.reset(new dll_controller(
            resources_, trash_, *logger_, dir_path, dll_name));
    }
    catch (std::exception& e)
    {
        // e's actual type may be defined in the DLL. If so, its code may
        // disappear in controller's destructor, so throw a fresh exception
        // from here, containing a copy of e.what().
        std::string what{e.what()};
        logger_->error("loading DLL {} failed: {}", dll_name, what);
        throw dll_load_error{what};
    }
    controllers_.insert(std::make_pair(dll_name, std::move(controller)));
}

void
dll_collection::unload(std::string const& dll_name)
{
    std::scoped_lock lock{mutex_};
    auto it{controllers_.find(dll_name)};
    if (it == controllers_.end())
    {
        throw dll_unload_error{
            fmt::format("no DLL loaded named {}", dll_name)};
    }
    controllers_.erase(it);
}

} // namespace cradle
