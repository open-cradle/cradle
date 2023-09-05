#ifndef CRADLE_INNER_DLL_DLL_SINGLETON_H
#define CRADLE_INNER_DLL_DLL_SINGLETON_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <cradle/inner/dll/dll_controller.h>

namespace cradle {

// Singleton representing the set of all loaded DLLs
class dll_singleton
{
 public:
    static dll_singleton&
    instance();

    void
    add(std::unique_ptr<dll_controller> controller);

    dll_controller*
    find(std::string const& dll_name);

    std::unique_ptr<dll_controller>
    remove(std::string const& dll_name);

 private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<dll_controller>>
        controllers_;
};

} // namespace cradle

#endif
