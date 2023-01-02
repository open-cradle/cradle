#ifndef CRADLE_PLUGINS_SERIALIZATION_RESPONSE_MSGPACK_H
#define CRADLE_PLUGINS_SERIALIZATION_RESPONSE_MSGPACK_H

// A plugin serializing responses (resulting from resolving requests),
// using MessagePack

#include <sstream>

#include <msgpack.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/msgpack_adaptors_main.h>

namespace cradle {

template<typename Value>
blob
serialize_response(Value const& value)
{
    std::stringstream ss;
    msgpack::pack(ss, value);
    return make_blob(ss.str());
}

template<typename Value>
Value
deserialize_response(blob const& x)
{
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
