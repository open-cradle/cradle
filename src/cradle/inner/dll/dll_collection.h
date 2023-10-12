#ifndef CRADLE_INNER_DLL_DLL_COLLECTION_H
#define CRADLE_INNER_DLL_DLL_COLLECTION_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include <cradle/inner/dll/dll_controller.h>

namespace cradle {

/*
 * The set of all loaded DLLs
 */
class dll_collection
{
 public:
    dll_collection();

    // Loads a shared library and registers its seri resolvers.
    //
    // dir_path is an absolute path to the directory containg the shared
    // library file.
    // dll_name is the library name as specified in CMakeLists.txt.
    // On Linux, dll_name "bla" translates to file name "libbla.so";
    // on Windows, it would be "bla.dll".
    void
    load(std::string const& dir_path, std::string const& dll_name);

    // Unloads a shared library and unregisters its seri resolvers.
    //
    // dll_name is as for load_shared_library(), or a regex if it contains "*".
    void
    unload(std::string const& dll_name);

 private:
    std::shared_ptr<spdlog::logger> logger_;
    std::mutex mutex_;
    // trash_ must outlive dll_controller objects so must appear before
    // controllers_
    dll_trash trash_;
    std::unordered_map<std::string, std::unique_ptr<dll_controller>>
        controllers_;

    void
    remove_one(std::string const& dll_name);

    void
    remove_matching(std::string const& dll_name);
};

} // namespace cradle

#endif
