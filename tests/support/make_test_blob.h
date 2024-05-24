#ifndef CRADLE_TESTS_SUPPORT_MAKE_TEST_BLOB_H
#define CRADLE_TESTS_SUPPORT_MAKE_TEST_BLOB_H

#include <string>

#include <cradle/inner/core/type_definitions.h>

namespace cradle {

class local_context_intf;

blob
make_test_blob(
    local_context_intf& ctx,
    std::string const& contents,
    bool use_shared_memory);

} // namespace cradle

#endif
