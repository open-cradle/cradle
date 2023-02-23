#include <map>
#include <stdexcept>
#include <string>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/caching/secondary_cache_intf.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/encodings/base64.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

static std::map<std::string, std::unique_ptr<secondary_cache_factory>>
    secondary_cache_factories;

void
register_secondary_cache_factory(
    std::string const& key, std::unique_ptr<secondary_cache_factory> factory)
{
    secondary_cache_factories.emplace(key, std::move(factory));
}

static immutable_cache_config
make_immutable_cache_config(service_config const& config)
{
    return immutable_cache_config{
        .unused_size_limit = config.get_number_or_default(
            inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00)};
}

void
inner_resources::inner_initialize(service_config const& config)
{
    create_memory_cache(config);
    create_secondary_cache(config);
    blob_dir_ = std::make_unique<blob_file_directory>(config);
}

void
inner_resources::create_memory_cache(service_config const& config)
{
    memory_cache_ = std::make_unique<immutable_cache>(
        make_immutable_cache_config(config));
}

void
inner_resources::create_secondary_cache(service_config const& config)
{
    auto key = config.get_mandatory_string(
        inner_config_keys::SECONDARY_CACHE_FACTORY);
    auto factory = secondary_cache_factories.find(key);
    if (factory == secondary_cache_factories.end())
    {
        throw config_error{
            fmt::format("No secondary cache factory \"{}\"", key)};
    }
    secondary_cache_ = factory->second->create(config);
}

void
inner_resources::inner_reset_memory_cache()
{
    service_config_map config_map;
    inner_reset_memory_cache(service_config(config_map));
}

void
inner_resources::inner_reset_memory_cache(service_config const& config)
{
    memory_cache_->reset(make_immutable_cache_config(config));
}

void
inner_resources::inner_reset_secondary_cache(service_config const& config)
{
    secondary_cache_->reset(config);
}

std::shared_ptr<blob_file_writer>
inner_resources::make_blob_file_writer(std::size_t size)
{
    auto path{blob_dir_->allocate_file()};
    return std::make_shared<blob_file_writer>(path, size);
}

} // namespace cradle
