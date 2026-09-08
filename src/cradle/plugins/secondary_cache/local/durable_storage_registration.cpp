#include <cradle/plugins/secondary_cache/local/durable_storage_registration.h>

#include <map>
#include <memory>
#include <string>

#include <fmt/format.h>

#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/memory_ac.h>
#include <cradle/inner/storage/memory_cas.h>
#include <cradle/inner/storage/memory_mutable_store.h>
#include <cradle/inner/storage/storage_config_keys.h>
#include <cradle/plugins/secondary_cache/local/disk_ac.h>
#include <cradle/plugins/secondary_cache/local/disk_cas.h>
#include <cradle/plugins/secondary_cache/local/disk_mutable_store.h>
#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

namespace cradle {

// Returns the shared durable store for a capability's resolved directory,
// building it lazily and reusing an already-built store for the same
// directory (dedup strictly by resolved directory).
static std::shared_ptr<local_durable_store>
durable_store_for_capability(
    service_config const& config,
    std::string const& capability,
    std::map<std::string, std::shared_ptr<local_durable_store>>& stores)
{
    auto const settings{resolve_durable_store_settings(config, capability)};
    if (auto const it{stores.find(settings.directory)}; it != stores.end())
    {
        return it->second;
    }
    auto store{make_local_durable_store(settings)};
    stores.emplace(settings.directory, store);
    return store;
}

void
register_local_durable_storage_from_config(inner_resources& resources)
{
    auto const& config{resources.config()};

    // One local_durable_store per distinct resolved directory, built lazily
    // and shared across capabilities that resolve to the same directory.
    std::map<std::string, std::shared_ptr<local_durable_store>> stores;

    if (auto const key{
            config.get_optional_string(storage_config_keys::CAS_FACTORY)})
    {
        if (*key == storage_config_values::MEMORY_CAS)
        {
            resources.set_cas_store(make_memory_cas(*key), true);
        }
        else if (*key == durable_storage_config_values::DISK_CAS)
        {
            auto store{durable_store_for_capability(config, "cas", stores)};
            resources.set_cas_store(make_disk_cas(store, *key), true);
        }
        else
        {
            throw config_error{fmt::format("no CAS backend named {}", *key)};
        }
    }

    if (auto const key{
            config.get_optional_string(storage_config_keys::AC_FACTORY)})
    {
        if (*key == storage_config_values::MEMORY_AC)
        {
            resources.set_ac_store(make_memory_ac(*key), true);
        }
        else if (*key == durable_storage_config_values::DISK_AC)
        {
            auto store{durable_store_for_capability(config, "ac", stores)};
            resources.set_ac_store(make_disk_ac(store, *key), true);
        }
        else
        {
            throw config_error{fmt::format("no AC backend named {}", *key)};
        }
    }

    if (auto const key{config.get_optional_string(
            storage_config_keys::MUTABLE_STORE_FACTORY)})
    {
        if (*key == storage_config_values::MEMORY_MUTABLE_STORE)
        {
            resources.set_mutable_store(make_memory_mutable_store(*key), true);
        }
        else if (*key == durable_storage_config_values::DISK_MUTABLE_STORE)
        {
            auto store{
                durable_store_for_capability(config, "mutable_store", stores)};
            resources.set_mutable_store(
                make_disk_mutable_store(store, *key), true);
        }
        else
        {
            throw config_error{
                fmt::format("no mutable store backend named {}", *key)};
        }
    }
}

} // namespace cradle
