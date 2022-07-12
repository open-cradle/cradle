#ifndef CRADLE_TYPING_CORE_UNIQUE_HASH_H
#define CRADLE_TYPING_CORE_UNIQUE_HASH_H

#include <concepts>

#include <cradle/inner/core/id.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/typing/encodings/native.h>

namespace cradle {

template<typename Value>
requires(!(std::integral<Value> || std::floating_point<Value>) ) void update_unique_hash(
    unique_hasher& hasher, Value const& value)
{
    auto natively_encoded = write_natively_encoded_value(to_dynamic(value));
    // The value already contains the type, so no need to store it separately.
    hasher.encode_value(natively_encoded.begin(), natively_encoded.end());
}

} // namespace cradle

#endif
