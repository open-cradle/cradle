#include <fmt/format.h>

#include <cradle/plugins/secondary_cache/all_plugins.h>
#include <cradle/plugins/secondary_cache/http/http_cache.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>

namespace cradle {

// TODO consider adding the "simple" plugins
std::vector<std::string>
get_secondary_storage_plugin_names()
{
    return std::vector<std::string>{
        http_cache_config_values::PLUGIN_NAME,
        local_disk_cache_config_values::PLUGIN_NAME,
    };
}

std::unique_ptr<secondary_storage_intf>
create_secondary_storage(inner_resources& resources)
{
    auto const& config{resources.config()};
    auto const& opt_key{config.get_optional_string(
        inner_config_keys::SECONDARY_CACHE_FACTORY)};
    if (!opt_key)
    {
        return nullptr;
    }
    auto const& key{*opt_key};
    if (key == local_disk_cache_config_values::PLUGIN_NAME)
    {
        return std::make_unique<local_disk_cache>(config);
    }
    else if (key == http_cache_config_values::PLUGIN_NAME)
    {
        return std::make_unique<http_cache>(resources);
    }
    throw config_error{fmt::format("no secondary storage named {}", key)};
}

} // namespace cradle
