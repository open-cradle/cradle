#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/encodings/base64.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/service/disk_cache_intf.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

static std::map<std::string, std::unique_ptr<disk_cache_factory>>
    disk_cache_factories;

void
register_disk_cache_factory(
    std::string const& key, std::unique_ptr<disk_cache_factory> factory)
{
    disk_cache_factories.emplace(key, std::move(factory));
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
    create_disk_cache(config);
}

void
inner_resources::create_memory_cache(service_config const& config)
{
    memory_cache_ = std::make_unique<immutable_cache>(
        make_immutable_cache_config(config));
}

void
inner_resources::create_disk_cache(service_config const& config)
{
    auto key
        = config.get_mandatory_string(inner_config_keys::DISK_CACHE_FACTORY);
    auto factory = disk_cache_factories.find(key);
    if (factory == disk_cache_factories.end())
    {
        std::ostringstream os;
        os << "No disk cache factory \"" << key << "\"";
        throw config_error(os.str());
    }
    disk_cache_ = factory->second->create(config);
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
inner_resources::inner_reset_disk_cache(service_config const& config)
{
    disk_cache_->reset(config);
}

} // namespace cradle
