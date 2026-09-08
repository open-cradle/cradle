#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

#include <cstdint>

#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>

namespace cradle {

namespace {

// Default I/O pool sizes when not otherwise configured.
constexpr uint32_t default_num_read_threads = 2;
constexpr unsigned default_num_write_threads = 2;

// The per-capability override keys applicable to one capability.
struct capability_override_keys
{
    std::string directory;
    std::string size_limit;
    std::string check_file_data;
};

capability_override_keys
override_keys_for(std::string const& capability)
{
    if (capability == "cas")
    {
        return {
            durable_storage_config_keys::CAS_DIRECTORY,
            durable_storage_config_keys::CAS_SIZE_LIMIT,
            durable_storage_config_keys::CAS_CHECK_FILE_DATA};
    }
    if (capability == "ac")
    {
        return {
            durable_storage_config_keys::AC_DIRECTORY,
            durable_storage_config_keys::AC_SIZE_LIMIT,
            durable_storage_config_keys::AC_CHECK_FILE_DATA};
    }
    if (capability == "mutable_store")
    {
        return {
            durable_storage_config_keys::MUTABLE_STORE_DIRECTORY,
            durable_storage_config_keys::MUTABLE_STORE_SIZE_LIMIT,
            durable_storage_config_keys::MUTABLE_STORE_CHECK_FILE_DATA};
    }
    throw config_error{"unknown durable-store capability: " + capability};
}

ll_disk_cache_config
make_ll_config(durable_store_settings const& settings)
{
    std::optional<std::string> directory;
    if (!settings.directory.empty())
    {
        directory = settings.directory;
    }
    return ll_disk_cache_config{
        directory, settings.size_limit, settings.start_empty};
}

} // namespace

durable_store_settings
resolve_durable_store_settings(
    service_config const& config, std::string const& capability)
{
    auto const keys = override_keys_for(capability);
    durable_store_settings settings;

    if (config.contains(keys.directory))
    {
        settings.directory = config.get_mandatory_string(keys.directory);
    }
    else
    {
        settings.directory = config.get_string_or_default(
            local_disk_cache_config_keys::DIRECTORY, "");
    }

    if (config.contains(keys.size_limit))
    {
        settings.size_limit = config.get_mandatory_number(keys.size_limit);
    }
    else
    {
        settings.size_limit = config.get_optional_number(
            local_disk_cache_config_keys::SIZE_LIMIT);
    }

    if (config.contains(keys.check_file_data))
    {
        settings.check_file_data
            = config.get_mandatory_bool(keys.check_file_data);
    }
    else
    {
        settings.check_file_data = config.get_bool_or_default(
            local_disk_cache_config_keys::CHECK_FILE_DATA, false);
    }

    // start_empty has no per-capability form; always the shared default.
    settings.start_empty = config.get_bool_or_default(
        local_disk_cache_config_keys::START_EMPTY, false);

    return settings;
}

local_durable_store::local_durable_store(service_config const& config)
    : check_file_data_{config.get_bool_or_default(
        local_disk_cache_config_keys::CHECK_FILE_DATA, false)},
      ll_cache_{ll_disk_cache_config{
          config.get_optional_string(local_disk_cache_config_keys::DIRECTORY),
          config.get_optional_number(local_disk_cache_config_keys::SIZE_LIMIT),
          config.get_bool_or_default(
              local_disk_cache_config_keys::START_EMPTY, false)}},
      read_pool_{default_num_read_threads},
      write_pool_{default_num_write_threads},
      logger_{spdlog::get("cradle")}
{
}

local_durable_store::local_durable_store(
    durable_store_settings const& settings)
    : check_file_data_{settings.check_file_data},
      ll_cache_{make_ll_config(settings)},
      read_pool_{default_num_read_threads},
      write_pool_{default_num_write_threads},
      logger_{spdlog::get("cradle")}
{
}

ll_disk_cache&
local_durable_store::cache()
{
    return ll_cache_;
}

cppcoro::static_thread_pool&
local_durable_store::read_pool()
{
    return read_pool_;
}

BS::thread_pool&
local_durable_store::write_pool()
{
    return write_pool_;
}

bool
local_durable_store::check_file_data() const
{
    return check_file_data_;
}

std::shared_ptr<local_durable_store>
make_local_durable_store(service_config const& config)
{
    return std::make_shared<local_durable_store>(config);
}

std::shared_ptr<local_durable_store>
make_local_durable_store(durable_store_settings const& settings)
{
    return std::make_shared<local_durable_store>(settings);
}

} // namespace cradle
