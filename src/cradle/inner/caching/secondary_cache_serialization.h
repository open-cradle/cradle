#ifndef CRADLE_INNER_CACHING_SECONDARY_CACHE_SERIALIZATION_H
#define CRADLE_INNER_CACHING_SECONDARY_CACHE_SERIALIZATION_H

#include <cradle/inner/core/type_definitions.h>

// Serialization interface for values stored in a secondary cache.
// The implementation will be provided by a plugin, and activated from the
// (test) application by #include'ing the .h provided by that plugin.
// Having templates depending on a typename necessitates some form of
// build-time binding.

namespace cradle {

// Serializes a value to be stored in the secondary cache.
// CRADLE inner only declares this template function; a plugin should
// provide its definition.
template<typename Value>
blob
serialize_secondary_cache_value(Value const& value);

// Deserializes a blob, read from the secondary cache, into a value.
// CRADLE inner only declares this template function; a plugin should
// provide its definition.
template<typename Value>
Value
deserialize_secondary_cache_value(blob const& x);

} // namespace cradle

#endif
