#ifndef CRADLE_INNER_CORE_GET_UNIQUE_STRING_H
#define CRADLE_INNER_CORE_GET_UNIQUE_STRING_H

#include <string>

#include <cradle/inner/core/unique_hash.h>

namespace cradle {

std::string
get_unique_string_tmpl(auto const& value)
{
    unique_hasher hasher;
    update_unique_hash(hasher, value);
    return hasher.get_string();
}

// Get a string that is unique for the given ID (based on its hash).
// The primary purpose of these strings is to act as keys in the disk cache.
// A disk cache item corresponds to a request, either old-style (Thinknode),
// or a new-style one.
// Preventing collisions between all possible disk cache keys is crucial.
//
// A new-style disk-cached request must have a uuid.
// - The uuid defines the class types of the request and its arguments, and
//   the same for any non-type-erased subrequests.
// - Any type-erased subrequest (even if not disk-cached) must also have a
//   uuid.
// - The collection of all these uuids defines the class types of all requests,
//   and all their arguments. So by making these uuids part of the hash, all
//   type information is recorded.
// - Non-type-erased subrequests could also have a uuid. Including them in
//   the hash is not really needed, but this optimization does not seem
//   worthwhile.
// - The hash must cover all argument values.
// - If an argument is some kind of variant like a dynamic, the hash must
//   include the discriminator; see typing/core/unique_hash.h.
//
// In case of an old-style Thinknode request, id must be a sha256_hashed_id
// calculated for that request.
//
// Trying to make this a specialization of get_unique_string_tmpl() doesn't
// work for some reason.
struct id_interface;

std::string
get_unique_string(id_interface const& id);

} // namespace cradle

#endif
