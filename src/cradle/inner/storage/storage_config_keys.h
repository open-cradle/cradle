#ifndef CRADLE_INNER_STORAGE_STORAGE_CONFIG_KEYS_H
#define CRADLE_INNER_STORAGE_STORAGE_CONFIG_KEYS_H

#include <string>

namespace cradle {

// Config keys selecting a storage backend/instance by name at construction
// (FR-22), following the local_disk_cache_config_keys / http_cache_config_keys
// convention (slash-namespaced strings grouped in a struct).
struct storage_config_keys
{
    // Selects the CAS backend/instance by name (FR-22).
    inline static std::string const CAS_FACTORY{"storage/cas_factory"};

    // Selects the AC backend/instance by name (FR-22).
    inline static std::string const AC_FACTORY{"storage/ac_factory"};

    // Selects the mutable-store backend/instance by name (FR-22).
    inline static std::string const MUTABLE_STORE_FACTORY{
        "storage/mutable_store_factory"};
};

// Config values naming the registered backends, following the
// *_config_values::PLUGIN_NAME convention. For M1 the only registered backend
// is the in-memory implementation (D6).
struct storage_config_values
{
    // Value selecting the in-memory CAS backend.
    inline static std::string const MEMORY_CAS{"memory_cas"};

    // Value selecting the in-memory AC backend.
    inline static std::string const MEMORY_AC{"memory_ac"};

    // Value selecting the in-memory mutable-store backend.
    inline static std::string const MEMORY_MUTABLE_STORE{
        "memory_mutable_store"};
};

} // namespace cradle

#endif
