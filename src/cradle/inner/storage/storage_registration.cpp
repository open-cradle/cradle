#include <cradle/inner/storage/storage_registration.h>

#include <fmt/format.h>

#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/memory_ac.h>
#include <cradle/inner/storage/memory_cas.h>
#include <cradle/inner/storage/memory_mutable_store.h>
#include <cradle/inner/storage/storage_config_keys.h>

namespace cradle {

void
register_storage_from_config(inner_resources& resources)
{
    auto const& config{resources.config()};

    if (auto const key{
            config.get_optional_string(storage_config_keys::CAS_FACTORY)})
    {
        if (*key == storage_config_values::MEMORY_CAS)
        {
            resources.set_cas_store(make_memory_cas(*key), true);
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
            resources.set_mutable_store(
                make_memory_mutable_store(*key), true);
        }
        else
        {
            throw config_error{
                fmt::format("no mutable store backend named {}", *key)};
        }
    }
}

} // namespace cradle
