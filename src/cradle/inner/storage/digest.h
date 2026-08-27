#ifndef CRADLE_INNER_STORAGE_DIGEST_H
#define CRADLE_INNER_STORAGE_DIGEST_H

#include <string>

#include <cradle/inner/core/type_definitions.h>

namespace cradle {

// A content digest, as produced by the content-hashing facility; a plain
// std::string for M1.
using digest = std::string;

using request_key = std::string;

using mutable_value = blob;

} // namespace cradle

#endif
