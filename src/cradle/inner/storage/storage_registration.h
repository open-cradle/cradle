#ifndef CRADLE_INNER_STORAGE_STORAGE_REGISTRATION_H
#define CRADLE_INNER_STORAGE_STORAGE_REGISTRATION_H

namespace cradle {

class inner_resources;

// Reads the storage_config_keys from the resources' service_config and, for
// each key that selects the in-memory backend, constructs the matching store
// (via make_memory_*) and registers it on inner_resources under its name as
// the default instance (FR-20, FR-22). An absent key registers nothing;
// an unrecognized backend name throws (config_error).
void
register_storage_from_config(inner_resources& resources);

} // namespace cradle

#endif
