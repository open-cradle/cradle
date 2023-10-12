#include <regex>
#include <stdexcept>

#include <fmt/format.h>

#include <cradle/inner/dll/dll_collection.h>
#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/dll/dll_exceptions.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

dll_collection::dll_collection() : logger_{ensure_logger("dll")}
{
}

void
dll_collection::load(std::string const& dir_path, std::string const& dll_name)
{
    std::scoped_lock lock{mutex_};
    if (controllers_.contains(dll_name))
    {
        logger_->warn("DLL {} already loaded", dll_name);
        return;
    }
#ifdef _WIN32
    std::string dll_path{fmt::format("{}/{}.dll", dir_path, dll_name)};
#else
    std::string dll_path{fmt::format("{}/lib{}.so", dir_path, dll_name)};
#endif
    std::unique_ptr<dll_controller> controller;
    try
    {
        controller.reset(
            new dll_controller(trash_, std::move(dll_path), dll_name));
    }
    catch (std::exception& e)
    {
        // e's actual type may be defined in the DLL. If so, its code may
        // disappear in controller's destructor, so throw a fresh exception
        // from here, containing a copy of e.what().
        std::string what{e.what()};
        logger_->error(
            "load_shared_library({}, {}) failed: {}",
            dir_path,
            dll_name,
            what);
        throw dll_load_error{what};
    }
    controllers_.insert(std::make_pair(dll_name, std::move(controller)));
}

void
dll_collection::unload(std::string const& dll_name)
{
    std::scoped_lock lock{mutex_};
    if (dll_name.find('*') == std::string::npos)
    {
        remove_one(dll_name);
    }
    else
    {
        remove_matching(dll_name);
    }
}

void
dll_collection::remove_one(std::string const& dll_name)
{
    auto it{controllers_.find(dll_name)};
    if (it == controllers_.end())
    {
        throw std::out_of_range{
            fmt::format("no DLL loaded named {}", dll_name)};
    }
    controllers_.erase(it);
}

void
dll_collection::remove_matching(std::string const& dll_name_regex)
{
    std::regex re{dll_name_regex};
    for (auto it = controllers_.begin(); it != controllers_.end();)
    {
        if (std::regex_match(it->first, re))
        {
            it = controllers_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace cradle
