#ifndef CRADLE_INNER_SERVICE_DISK_CACHE_SERIALIZATION_H
#define CRADLE_INNER_SERVICE_DISK_CACHE_SERIALIZATION_H

// Disk cache serialization interface.
// The implementation will be provided by a plugin, and activated from the
// (test) application by #include'ing the .h provided by that plugin.
// Having templates depending on a typename necessitates some form of
// build-time binding.

namespace cradle {

// Serializes a value to be stored in the disk cache.
// CRADLE inner only declares this template function; a plugin should
// provide its definition.
template<typename Value>
blob
serialize_disk_cache_value(Value const& value);

// Deserializes a blob, read from the disk cache, into a value.
// CRADLE inner only declares this template function; a plugin should
// provide its definition.
template<typename Value>
Value
deserialize_disk_cache_value(blob const& x);

} // namespace cradle

#endif
