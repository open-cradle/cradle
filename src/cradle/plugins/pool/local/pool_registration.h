#ifndef CRADLE_PLUGINS_POOL_LOCAL_POOL_REGISTRATION_H
#define CRADLE_PLUGINS_POOL_LOCAL_POOL_REGISTRATION_H

namespace cradle {

class inner_resources;

// Reads pool_config_keys from the resources' service_config and, when a pool
// is selected, resolves the named CAS / AC / mutable-store instances from
// inner_resources (the defaults when the binding keys are absent), reads the
// input-size boundary and poll cadence (built-in defaults when absent),
// constructs a local_pool via make_local_pool, and registers it on
// inner_resources under its configured name as the default pool. An absent
// pool/factory key registers nothing; an unrecognized value throws
// (config_error). Storage registration must have run first so the named
// instances exist.
void
register_local_pool_from_config(inner_resources& resources);

} // namespace cradle

#endif
