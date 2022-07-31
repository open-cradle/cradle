#ifndef CRADLE_TYPING_CORE_UNIQUE_HASH_H
#define CRADLE_TYPING_CORE_UNIQUE_HASH_H

#include <cradle/inner/core/id.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/typing/encodings/native.h>

namespace cradle {

// TODO is this a primary template that should be declared _before_ any
// specialization?
template<typename Value>
void
update_unique_hash(unique_hasher& hasher, Value const& value)
{
    auto natively_encoded = write_natively_encoded_value(to_dynamic(value));
    // TODO prefix with something denoting "natively encoded"
    auto begin = &*natively_encoded.begin();
    auto end = begin + natively_encoded.size();
    hasher.encode_bytes(begin, end);
}

} // namespace cradle

#endif
