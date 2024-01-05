#ifndef CRADLE_TYPING_CORE_UNIQUE_HASH_H
#define CRADLE_TYPING_CORE_UNIQUE_HASH_H

#include <cradle/inner/core/id.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/typing/encodings/native.h>

namespace cradle {

// This should be the least-special, last-resort definition.
// Only for dynamic's?
template<typename T>
    requires(!(std::integral<T> || std::floating_point<T>) )
void update_unique_hash(unique_hasher& hasher, T const& value)
{
    auto natively_encoded = write_natively_encoded_value(to_dynamic(value));
    auto begin = &*natively_encoded.begin();
    auto end = begin + natively_encoded.size();
    hasher.encode_bytes(begin, end);
}

} // namespace cradle

#endif
