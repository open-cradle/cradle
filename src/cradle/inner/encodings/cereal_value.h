#ifndef CRADLE_INNER_ENCODINGS_CEREAL_VALUE_H
#define CRADLE_INNER_ENCODINGS_CEREAL_VALUE_H

// Serialization of any value to/from a blob, using cereal.
// Current usage:
// - Secondary cache

#include <sstream>
#include <type_traits>

#include <cereal/archives/binary.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>

namespace cradle {

template<typename Value>
blob
serialize_value(Value const& value)
{
    if constexpr (std::same_as<Value, blob>)
    {
        // The serialization/deserialization process is unnecessary for blobs.
        return value;
    }
    else
    {
        std::stringstream ss;
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(value);
        return make_blob(ss.str());
    }
}

template<typename Value>
Value
deserialize_value(blob const& x)
{
    if constexpr (std::same_as<Value, blob>)
    {
        // The serialization/deserialization process is unnecessary for blobs.
        return x;
    }
    else
    {
        auto data = reinterpret_cast<char const*>(x.data());
        std::stringstream is;
        // The string constructor will copy the blob data but this looks
        // unavoidable.
        is.str(std::string(data, x.size()));
        cereal::BinaryInputArchive iarchive(is);
        Value res;
        iarchive(res);
        return res;
    }
}

} // namespace cradle

#endif
