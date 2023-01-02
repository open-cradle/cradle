#include "inner_service.h"
#include <cradle/inner/fs/types.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache_plugin.h>

namespace cradle {

void
init_test_inner_service(inner_resources& resources)
{
    activate_local_disk_cache_plugin();

    std::string cache_dir{"tests_inner_disk_cache"};
    reset_directory(file_path(cache_dir));

    service_config_map config_map{
        {inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00U},
        {inner_config_keys::DISK_CACHE_FACTORY,
         local_disk_cache_config_values::PLUGIN_NAME},
        {local_disk_cache_config_keys::DIRECTORY, cache_dir},
        {local_disk_cache_config_keys::SIZE_LIMIT, 0x40'00'00'00U},
        {local_disk_cache_config_keys::NUM_THREADS_READ_POOL, 2U},
        {local_disk_cache_config_keys::NUM_THREADS_WRITE_POOL, 2U},
    };
    resources.inner_initialize(service_config(config_map));
}

void
inner_reset_disk_cache(inner_resources& resources)
{
    service_config_map config_map{
        {local_disk_cache_config_keys::DIRECTORY, "tests_inner_disk_cache"},
        {local_disk_cache_config_keys::SIZE_LIMIT, 0x40'00'00'00U},
        {local_disk_cache_config_keys::NUM_THREADS_READ_POOL, 2U},
        {local_disk_cache_config_keys::NUM_THREADS_WRITE_POOL, 2U},
    };
    resources.inner_reset_disk_cache(service_config(config_map));
}

cached_request_resolution_context::cached_request_resolution_context()
{
    init_test_inner_service(resources);
}

void
cached_request_resolution_context::reset_memory_cache()
{
    service_config_map config_map{
        {inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00U},
    };
    resources.inner_reset_memory_cache(service_config(config_map));
}

void
ensure_loopback_service()
{
    static bool registered{false};
    if (!registered)
    {
        register_loopback_service();
        registered = true;
    }
}

} // namespace cradle
