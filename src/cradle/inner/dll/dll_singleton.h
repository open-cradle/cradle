#ifndef CRADLE_INNER_DLL_DLL_SINGLETON_H
#define CRADLE_INNER_DLL_DLL_SINGLETON_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

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

    // Removes controllers matching dll_name, and transfers their ownership to
    // the caller.
    // If dll_name is a simple name, returns exactly one controller.
    // If dll_name contains a "*", the size of the returned vector can be 0, 1
    // or more.
    std::vector<std::unique_ptr<dll_controller>>
    remove(std::string const& dll_name);

 private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<dll_controller>>
        controllers_;

    std::vector<std::unique_ptr<dll_controller>>
    remove_one(std::string const& dll_name);

    std::vector<std::unique_ptr<dll_controller>>
    remove_matching(std::string const& dll_name);
};

} // namespace cradle

#endif
