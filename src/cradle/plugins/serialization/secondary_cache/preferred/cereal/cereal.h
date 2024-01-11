#ifndef CRADLE_PLUGINS_SERIALIZATION_SECONDARY_CACHE_PREFERRED_CEREAL_CEREAL_H
#define CRADLE_PLUGINS_SERIALIZATION_SECONDARY_CACHE_PREFERRED_CEREAL_CEREAL_H

// A plugin serializing disk-cached values using cereal.

#include <sstream>
#include <type_traits>

#include <cereal/archives/binary.hpp>

#include <cradle/inner/caching/secondary_cache_serialization.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>

namespace cradle {

template<typename Value>
blob
serialize_secondary_cache_value(Value const& value)
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
deserialize_secondary_cache_value(blob const& x)
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
