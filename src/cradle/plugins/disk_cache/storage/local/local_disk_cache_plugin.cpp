#include <cradle/inner/service/resources.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache_factory.h>

namespace cradle {

void
activate_local_disk_cache_plugin()
{
    register_disk_cache_factory(
        local_disk_cache_config_values::PLUGIN_NAME,
        std::make_unique<local_disk_cache_factory>());
}

} // namespace cradle
