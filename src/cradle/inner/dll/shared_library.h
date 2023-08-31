#ifndef CRADLE_INNER_DLL_SHARED_LIBRARY_H
#define CRADLE_INNER_DLL_SHARED_LIBRARY_H

// TODO should this be in inner? Used by rpclib server only.

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
 * The DLL must export (at least) these functions:
 * - void CRADLE_init()
 *   Must be called by the load_shared_library() implementation before
 *   accessing anything else inside the DLL. It will populate the DLL's
 *   seri catalog.
 * - seri_catalog* CRADLE_get_catalog()
 *   Returns a pointer to a static seri_catalog instance which was populated
 *   by CRADLE_init().
 */
void
load_shared_library(std::string const& dir_path, std::string const& dll_name);

} // namespace cradle

#endif
