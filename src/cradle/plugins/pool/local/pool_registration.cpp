#include <cradle/plugins/pool/local/pool_registration.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include <fmt/format.h>

#include <cradle/inner/pool/pool_config_keys.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/mutable_store_intf.h>
#include <cradle/plugins/pool/local/local_pool.h>

namespace cradle {

namespace {

// Built-in defaults applied when the corresponding pool/* key is absent.
constexpr std::size_t default_input_size_boundary{1024};
constexpr int default_poll_interval_ms{1};
constexpr int default_poll_max_interval_ms{100};

// Resolves a named storage instance when its binding key is present, otherwise
// the default instance.
cas_intf&
resolve_cas_store(inner_resources& resources, service_config const& config)
{
    if (auto const name{
            config.get_optional_string(pool_config_keys::CAS_STORE)})
    {
        return resources.cas_store(*name);
    }
    return resources.cas_store();
}

ac_intf&
resolve_ac_store(inner_resources& resources, service_config const& config)
{
    if (auto const name{
            config.get_optional_string(pool_config_keys::AC_STORE)})
    {
        return resources.ac_store(*name);
    }
    return resources.ac_store();
}

mutable_store_intf&
resolve_mutable_store(inner_resources& resources, service_config const& config)
{
    if (auto const name{
            config.get_optional_string(pool_config_keys::MUTABLE_STORE)})
    {
        return resources.mutable_store(*name);
    }
    return resources.mutable_store();
}

pool_settings
resolve_pool_settings(service_config const& config)
{
    pool_settings settings;
    settings.input_size_boundary = config.get_number_or_default(
        pool_config_keys::INPUT_SIZE_BOUNDARY, default_input_size_boundary);
    settings.poll_interval
        = std::chrono::milliseconds{config.get_number_or_default(
            pool_config_keys::POLL_INTERVAL_MS, default_poll_interval_ms)};
    settings.poll_max_interval
        = std::chrono::milliseconds{config.get_number_or_default(
            pool_config_keys::POLL_MAX_INTERVAL_MS,
            default_poll_max_interval_ms)};
    return settings;
}

} // namespace

void
register_local_pool_from_config(inner_resources& resources)
{
    auto const& config{resources.config()};

    auto const factory{config.get_optional_string(pool_config_keys::FACTORY)};
    if (!factory)
    {
        return;
    }

    if (*factory != pool_config_values::LOCAL_POOL)
    {
        throw config_error{fmt::format("no pool backend named {}", *factory)};
    }

    auto const name{
        config.get_string_or_default(pool_config_keys::NAME, *factory)};
    auto& cas{resolve_cas_store(resources, config)};
    auto& ac{resolve_ac_store(resources, config)};
    auto& mutable_store{resolve_mutable_store(resources, config)};
    auto settings{resolve_pool_settings(config)};

    auto pool{make_local_pool(
        name, resources, cas, ac, mutable_store, std::move(settings))};
    resources.set_pool(std::move(pool), true);
}

} // namespace cradle
