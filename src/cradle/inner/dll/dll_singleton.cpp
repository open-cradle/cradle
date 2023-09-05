#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/dll/dll_singleton.h>

namespace cradle {

dll_singleton&
dll_singleton::instance()
{
    static dll_singleton the_instance;
    return the_instance;
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

std::unique_ptr<dll_controller>
dll_singleton::remove(std::string const& dll_name)
{
    std::scoped_lock lock{mutex_};
    auto it{controllers_.find(dll_name)};
    auto res{std::move(it->second)};
    controllers_.erase(it);
    return res;
}

} // namespace cradle
