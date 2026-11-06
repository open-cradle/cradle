#ifndef CRADLE_INNER_POOL_POOL_CONFIG_KEYS_H
#define CRADLE_INNER_POOL_POOL_CONFIG_KEYS_H

#include <string>

namespace cradle {

// Config keys selecting and configuring a pool by name, following the
// storage_config_keys convention.
struct pool_config_keys
{
    // Selects the pool backend/instance by value name.
    inline static std::string const FACTORY{"pool/factory"};

    // Instance name the pool reports from name() and is selected by.
    inline static std::string const NAME{"pool/name"};

    // Names of the CAS / AC / mutable-store instances the pool refers to. When
    // absent, the pool binds to the default named instances.
    inline static std::string const CAS_STORE{"pool/cas_store"};
    inline static std::string const AC_STORE{"pool/ac_store"};
    inline static std::string const MUTABLE_STORE{"pool/mutable_store"};

    // Serialized-size boundary for input classification, in bytes. Optional;
    // a built-in default applies when absent.
    inline static std::string const INPUT_SIZE_BOUNDARY{
        "pool/input_size_boundary"};

    // Status-observation poll cadence bounds, in milliseconds. Optional; a
    // built-in default applies when absent.
    inline static std::string const POLL_INTERVAL_MS{"pool/poll_interval_ms"};
    inline static std::string const POLL_MAX_INTERVAL_MS{
        "pool/poll_max_interval_ms"};
};

// Config values naming the registered pool backends, following the
// *_config_values::PLUGIN_NAME convention. For M2 the only registered backend
// is the same-machine local pool.
struct pool_config_values
{
    inline static std::string const LOCAL_POOL{"local_pool"};
};

} // namespace cradle

#endif
