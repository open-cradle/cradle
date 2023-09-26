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
//
// Currently, a DLL is deactivated rather than unloaded. Its resolvers become
// unavailable for new requests, but the DLL code stays in memory, as there
// might still be references to the code that could lead to a crash of the
// entire application. At least the following remain:
// - The immutable cache contains shared_ptr entries wrapped in an std::any.
//   When an entry is deleted, the shared_ptr's destructor is called, which
//   probably is code in the DLL.
// - A request object's function resides in the DLL.
// Keeping track of these references is possible but not without cost. In
// particular, a function_request_erased constructor would need to translate
// its uuid to some catalog reference, and increase a reference count. This
// means locking a mutex, whereas creating a request object currently is
// relatively cheap.
class dll_singleton
{
 public:
    static dll_singleton&
    instance();

    void
    add(std::unique_ptr<dll_controller> controller);

    dll_controller*
    find(std::string const& dll_name);

    // Removes controllers matching dll_name, and transfers their references to
    // the caller. Controller ownership stays with the dll_singleton.
    // If dll_name is a simple name, returns exactly one controller.
    // If dll_name contains a "*", the size of the returned vector can be 0, 1
    // or more.
    std::vector<dll_controller*>
    remove(std::string const& dll_name);

 private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<dll_controller>>
        controllers_;
    // Controllers that have been unloaded. They can no longer be accessed,
    // but for safety reasons the DLL is not unloaded.
    // There is a deliberate memory leak here. ~dll_controller() unloads
    // the DLL, and if any other objects exist referring to code in the DLL,
    // then calling those objects' destructors would crash the application.
    std::vector<std::unique_ptr<dll_controller>>* inactive_controllers_;

    dll_singleton();

    std::vector<dll_controller*>
    remove_one(std::string const& dll_name);

    std::vector<dll_controller*>
    remove_matching(std::string const& dll_name);
};

} // namespace cradle

#endif
