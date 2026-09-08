#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DURABLE_STORAGE_REGISTRATION_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DURABLE_STORAGE_REGISTRATION_H

namespace cradle {

class inner_resources;

// Reads storage_config_keys (CAS_FACTORY / AC_FACTORY / MUTABLE_STORE_FACTORY)
// from the resources' service_config and registers the selected backend for
// each capability on inner_resources as the default instance:
//   - a memory_* value delegates to the inner make_memory_* factory;
//   - a disk_* value resolves that capability's effective directory (the
//     per-capability override when present, otherwise the shared default) and
//     constructs the matching durable adapter over the local_durable_store for
//     that resolved directory. Stores are deduplicated by resolved directory:
//     a store already built for the same directory is reused, and a new one is
//     built only for a not-yet-seen directory. The adapter is then registered
//     via set_cas_store / set_ac_store / set_mutable_store.
// An absent key registers nothing for that capability; an unrecognized value
// throws (config_error). This is the durable-capable superset of the inner
// register_storage_from_config; composition roots that support durable storage
// call this instead of the inner dispatcher (the inner dispatcher and its M1
// tests remain unchanged for memory-only contexts).
void
register_local_durable_storage_from_config(inner_resources& resources);

} // namespace cradle

#endif
