#ifndef CRADLE_INNER_ENCODINGS_CEREAL_VALUE_H
#define CRADLE_INNER_ENCODINGS_CEREAL_VALUE_H

// Serialization of data types using cereal. The data type should already be
// serializable via msgpack.

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/encodings/cereal.h>
#include <cradle/inner/encodings/msgpack_value.h>

namespace cradle {

// A data type wanting cereal support via the mechanism implemented here should
// specialize this struct, setting value to true.
template<typename Value>
struct serializable_via_cereal
{
    static constexpr bool value = false;
};

template<typename Archive, typename Value>
void
save(Archive& archive, Value const& val)
    requires serializable_via_cereal<Value>::value
{
    // TODO convert to blob file if Value wants that?
    bool allow_blob_files{true};
    save(archive, serialize_value(val, allow_blob_files));
}

template<typename Archive, typename Value>
void
load(Archive& archive, Value& val)
    requires serializable_via_cereal<Value>::value
{
    blob b;
    load(archive, b);
    val = deserialize_value<Value>(b);
}

} // namespace cradle

#endif
