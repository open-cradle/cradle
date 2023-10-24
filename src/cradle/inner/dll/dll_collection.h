#ifndef CRADLE_INNER_DLL_DLL_COLLECTION_H
#define CRADLE_INNER_DLL_DLL_COLLECTION_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include <cradle/inner/dll/dll_trash_impl.h>

namespace cradle {

class dll_controller;
class inner_resources;

/*
 * The set of all loaded DLLs
 *
 * All functions are thread-safe.
 */
class dll_collection
{
 public:
    // The inner_resources object may own this dll_collection. It must outlive
    // the dll_collection object.
    // Currently, only the seri_registry resource is used, but future DLL
    // functionality might require additional resources.
    dll_collection(inner_resources& resources);

    ~dll_collection();

    // Loads a shared library and registers its seri resolvers (if any).
    //
    // dir_path is an absolute path to the directory containg the shared
    // library file.
    // dll_name is the library name as specified in CMakeLists.txt.
    // On Linux, dll_name "bla" translates to file name "libbla.so";
    // on Windows, it would be "bla.dll".
    void
    load(std::string const& dir_path, std::string const& dll_name);

    // Unloads a shared library and unregisters its seri resolvers (if any).
    //
    // dll_name is as for load_shared_library(), or a regex if it contains "*".
    void
    unload(std::string const& dll_name);

    // Returns the number of loaded DLLs.
    auto
    size() const
    {
        std::scoped_lock lock{mutex_};
        return std::ssize(controllers_);
    }

    // Returns the number of DLLs in the trash. These are logically unloaded,
    // but still present in memory.
    auto
    trash_size() const
    {
        std::scoped_lock lock{mutex_};
        return trash_.size();
    }

 private:
    inner_resources& resources_;
    std::shared_ptr<spdlog::logger> logger_;
    mutable std::mutex mutex_;
    // trash_ must outlive dll_controller objects so must appear before
    // controllers_
    dll_trash_impl trash_;
    // dll_controller objects, identified by dll_name
    std::unordered_map<std::string, std::unique_ptr<dll_controller>>
        controllers_;

    void
    remove_one(std::string const& dll_name);

    void
    remove_matching(std::string const& dll_name);
};

} // namespace cradle

#endif
