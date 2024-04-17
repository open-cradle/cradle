#ifndef CRADLE_INNER_ENCODINGS_MSGPACK_VALUE_H
#define CRADLE_INNER_ENCODINGS_MSGPACK_VALUE_H

// Serialization of any value to/from a blob, using msgpack.
// Current usage:
// - Secondary cache
// - Serialized response (e.g., the value in an rpclib response)
// - Embedded in a cereal archive (see cereal_value.h)

#include <sstream>

#include <msgpack.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/msgpack_adaptors_main.h>

namespace cradle {

// Serializes a value to a msgpack-encoded byte sequence, stored in a blob
template<typename Value>
blob
serialize_value(Value const& value)
{
    if constexpr (std::same_as<Value, blob>)
    {
        // The serialization/deserialization process is unnecessary for blobs.
        // However, the deserialize_value() caller should not rely on the type
        // information that msgpack::pack() would normally add.
        return value;
    }
    std::stringstream ss;
    msgpack::pack(ss, value);
    return make_blob(std::move(ss).str());
}

// Deserializes a value from a msgpack-encoded byte sequence
template<typename Value>
Value
deserialize_value(blob const& x)
{
    if constexpr (std::same_as<Value, blob>)
    {
        // The serialization/deserialization process is unnecessary for blobs.
        return x;
    }
    Value resp;
    // TODO try to apply the ownership handle trick
    msgpack::object_handle oh
        = msgpack::unpack(reinterpret_cast<char const*>(x.data()), x.size());
    msgpack::object obj = oh.get();
    obj.convert(resp);
    return resp;
}

} // namespace cradle

#endif
