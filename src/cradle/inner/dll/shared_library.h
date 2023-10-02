#ifndef CRADLE_INNER_DLL_SHARED_LIBRARY_H
#define CRADLE_INNER_DLL_SHARED_LIBRARY_H

#include <string>

namespace cradle {

/*
 * Loads a shared library and registers its seri resolvers.
 *
 * dir_path is an absolute path to the directory containg the shared
 * library file.
 * dll_name is the library name as specified in CMakeLists.txt.
 * On Linux, dll_name "bla" translates to file name "libbla.so";
 * on Windows, it would be "bla.dll".
 *
 * The DLL must export (at least) this function:
 * - selfreg_seri_catalog* CRADLE_create_seri_catalog()
 *   Returns a pointer to a dynamically allocated selfreg_seri_catalog
 *   instance, transferring ownership of this object.
 *   Returns nullptr on error. As the selfreg_seri_catalog constructor is
 *   noexcept, this should be possible in an out-of-memory condition only.
 */
void
load_shared_library(std::string const& dir_path, std::string const& dll_name);

/*
 * Unloads a shared library and unregisters its seri resolvers.
 * The actual DLL unload must be postponed while references to its code exist.
 *
 * dll_name is as for load_shared_library(), or a regex if it contains "*".
 */
void
unload_shared_library(std::string const& dll_name);

} // namespace cradle

#endif
