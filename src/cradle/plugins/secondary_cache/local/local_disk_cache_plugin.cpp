#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache_factory.h>

namespace cradle {

void
activate_local_disk_cache_plugin(inner_resources& resources)
{
    resources.register_secondary_storage_factory(
        local_disk_cache_config_values::PLUGIN_NAME,
        std::make_unique<local_disk_cache_factory>());
}

} // namespace cradle
