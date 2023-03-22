#include <cradle/plugins/secondary_cache/all_plugins.h>
#include <cradle/plugins/secondary_cache/http/http_cache.h>
#include <cradle/plugins/secondary_cache/http/http_cache_plugin.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache_plugin.h>

namespace cradle {

std::vector<std::string>
get_secondary_storage_plugin_names()
{
    return std::vector<std::string>{
        http_cache_config_values::PLUGIN_NAME,
        local_disk_cache_config_values::PLUGIN_NAME,
    };
}

void
activate_all_secondary_storage_plugins()
{
    activate_http_cache_plugin();
    activate_local_disk_cache_plugin();
}

} // namespace cradle
