#ifndef CRADLE_PLUGINS_DISK_CACHE_SERIALIZATION_CEREAL_CEREAL_H
#define CRADLE_PLUGINS_DISK_CACHE_SERIALIZATION_CEREAL_CEREAL_H

// A plugin serializing disk-cached values using cereal.

#include <sstream>

#include <cereal/archives/binary.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/service/disk_cache_serialization.h>

namespace cradle {

template<typename Value>
blob
serialize_disk_cache_value(Value const& value)
{
    std::stringstream ss;
    cereal::BinaryOutputArchive oarchive(ss);
    oarchive(value);
    return make_blob(ss.str());
}

template<typename Value>
Value
deserialize_disk_cache_value(blob const& x)
{
    auto data = reinterpret_cast<char const*>(x.data());
    std::stringstream is;
    is.str(std::string(data, x.size()));
    cereal::BinaryInputArchive iarchive(is);
    Value res;
    iarchive(res);
    return res;
}

} // namespace cradle

#endif
