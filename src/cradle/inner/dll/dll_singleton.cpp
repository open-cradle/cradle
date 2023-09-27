#include <regex>
#include <stdexcept>

#include <fmt/format.h>

#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/dll/dll_singleton.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

dll_singleton&
dll_singleton::instance()
{
    static dll_singleton the_instance;
    return the_instance;
}

dll_singleton::dll_singleton()
    : inactive_controllers_{new inactive_dll_controllers()}
{
}

void
dll_singleton::add(std::unique_ptr<dll_controller> controller)
{
    std::string name{controller->name()};
    std::scoped_lock lock{mutex_};
    controllers_.insert(
        std::make_pair(std::move(name), std::move(controller)));
}

dll_controller*
dll_singleton::find(std::string const& dll_name)
{
    std::scoped_lock lock{mutex_};
    auto it{controllers_.find(dll_name)};
    if (it == controllers_.end())
    {
        return nullptr;
    }
    return &*it->second;
}

std::vector<dll_controller*>
dll_singleton::remove(std::string const& dll_name)
{
    std::scoped_lock lock{mutex_};
    if (dll_name.find('*') == std::string::npos)
    {
        return remove_one(dll_name);
    }
    else
    {
        return remove_matching(dll_name);
    }
}

std::vector<dll_controller*>
dll_singleton::remove_one(std::string const& dll_name)
{
    std::vector<dll_controller*> res;
    auto it{controllers_.find(dll_name)};
    if (it == controllers_.end())
    {
        throw std::out_of_range{
            fmt::format("no DLL loaded named {}", dll_name)};
    }
    res.push_back(&*it->second);
    inactive_controllers_->add(std::move(it->second));
    controllers_.erase(it);
    return res;
}

std::vector<dll_controller*>
dll_singleton::remove_matching(std::string const& dll_name_regex)
{
    std::vector<dll_controller*> res;
    std::regex re{dll_name_regex};
    for (auto it = controllers_.begin(); it != controllers_.end();)
    {
        if (std::regex_match(it->first, re))
        {
            res.push_back(&*it->second);
            inactive_controllers_->add(std::move(it->second));
            it = controllers_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    return res;
}

void
inactive_dll_controllers::add(std::unique_ptr<dll_controller> controller)
{
    controllers_.push_back(std::move(controller));
    auto logger{ensure_logger("dll")};
    logger->info("Now have {} inactive DLL controllers", controllers_.size());
}

} // namespace cradle
