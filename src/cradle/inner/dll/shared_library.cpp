#include <fmt/format.h>

#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/dll/dll_singleton.h>
#include <cradle/inner/dll/shared_library.h>

namespace cradle {

void
load_shared_library(std::string const& dir_path, std::string const& dll_name)
{
#ifdef _WIN32
    std::string dll_path{fmt::format("{}/{}.dll", dir_path, dll_name)};
#else
    std::string dll_path{fmt::format("{}/lib{}.so", dir_path, dll_name)};
#endif
    auto controller{
        std::make_unique<dll_controller>(std::move(dll_path), dll_name)};
    controller->load();
    dll_singleton::instance().add(std::move(controller));
}

} // namespace cradle
