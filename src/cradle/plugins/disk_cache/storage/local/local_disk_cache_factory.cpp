#include <cradle/inner/service/resources.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache_factory.h>

namespace cradle {

std::unique_ptr<disk_cache_intf>
local_disk_cache_factory::create(service_config const& config)
{
    return std::make_unique<local_disk_cache>(config);
}

} // namespace cradle
