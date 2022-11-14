#include "outer_service.h"
#include <cradle/inner/fs/types.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache_plugin.h>
#include <cradle/typing/service/core.h>

namespace cradle {

void
init_test_service(service_core& core)
{
    activate_local_disk_cache_plugin();

    std::string cache_dir{"service_disk_cache"};
    reset_directory(file_path(cache_dir));

    service_config_map config_map{
        {inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00U},
        {inner_config_keys::DISK_CACHE_FACTORY,
         local_disk_cache_config_values::PLUGIN_NAME},
        {local_disk_cache_config_keys::DIRECTORY, cache_dir},
        {local_disk_cache_config_keys::SIZE_LIMIT, 0x40'00'00'00U},
        {typing_config_keys::REQUEST_CONCURRENCY, 2U},
        {typing_config_keys::COMPUTE_CONCURRENCY, 2U},
        {typing_config_keys::HTTP_CONCURRENCY, 2U}};
    core.initialize(service_config(config_map));
}

} // namespace cradle
