#ifndef CRADLE_INNER_CORE_GET_UNIQUE_STRING_H
#define CRADLE_INNER_CORE_GET_UNIQUE_STRING_H

#include <string>

#include <cradle/inner/core/id.h>

namespace cradle {

// Get a string that is unique for the given ID (based on its hash).
// The primary purpose of these strings is to act as keys in the disk cache.
// A disk cache item corresponds to a request, either old-style (Thinknode),
// or a new-style one.
// Preventing collisions between all possible disk cache keys is crucial.
//
// A new-style disk-cached request must have a uuid.
// - The uuid defines the types of the request, any subrequests, and all
//   requests' arguments. So by making the uuid part of the hash, there is
//   no need to record any additional type information.
// - Subrequests could also have a uuid. As the top request's uuid already
//   defines the complete type, including the subrequests' uuids in the hash
//   is not really needed; this optimization doesn't seem worthwhile, though.
// - The hash must cover all argument values.
// - If an argument is some kind of variant like a dynamic, the hash must
//   include the discriminator; see typing/core/unique_hash.h.
//
// In case of an old-style Thinknode request, id must be a sha256_hashed_id
// calculated for that request.
std::string
get_unique_string(id_interface const& id);

} // namespace cradle

#endif
