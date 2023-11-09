#ifndef CRADLE_TESTS_THINKNODE_DLL_MAKE_SOME_BLOB_T0_MAKE_SOME_BLOB_T0_DEFS_H
#define CRADLE_TESTS_THINKNODE_DLL_MAKE_SOME_BLOB_T0_MAKE_SOME_BLOB_T0_DEFS_H

#include <string>

namespace cradle {

// Using thinknode_request_props and thinknode_proxy_props

// uuid to be qualified with caching level; must be "full" for remote
// resolution
static inline std::string const make_some_blob_t0_base_uuid{
    "make_some_blob-t0"};
static inline std::string const make_some_blob_t0_title{"test-make_some_blob"};

} // namespace cradle

#endif
