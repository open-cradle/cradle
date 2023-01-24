#include "inner_service.h"
#include <cradle/inner/fs/types.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache_plugin.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/registry.h>

namespace cradle {

static std::string tests_cache_dir{"tests_cache"};
static service_config_map const tests_config_map{
    {inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00U},
    {inner_config_keys::DISK_CACHE_FACTORY,
     local_disk_cache_config_values::PLUGIN_NAME},
    {local_disk_cache_config_keys::DIRECTORY, tests_cache_dir},
    {local_disk_cache_config_keys::SIZE_LIMIT, 0x40'00'00'00U},
    {local_disk_cache_config_keys::NUM_THREADS_READ_POOL, 2U},
    {local_disk_cache_config_keys::NUM_THREADS_WRITE_POOL, 2U},
    {blob_cache_config_keys::DIRECTORY, tests_cache_dir},
};

void
init_test_inner_service(inner_resources& resources)
{
    activate_local_disk_cache_plugin();
    reset_directory(file_path(tests_cache_dir));
    resources.inner_initialize(service_config(tests_config_map));
}

void
inner_reset_disk_cache(inner_resources& resources)
{
    resources.inner_reset_disk_cache(service_config(tests_config_map));
}

cached_request_resolution_context::cached_request_resolution_context()
{
    init_test_inner_service(resources);
}

void
cached_request_resolution_context::reset_memory_cache()
{
    resources.inner_reset_memory_cache(service_config(tests_config_map));
}

void
ensure_loopback_service()
{
    register_loopback_service();
}

std::shared_ptr<rpclib_client>
ensure_rpclib_service()
{
    // TODO no static here, add func to get previously registered client
    static std::shared_ptr<rpclib_client> client;
    if (!client)
    {
        // TODO pass non-default config?
        client = register_rpclib_client(service_config());
    }
    return client;
}

} // namespace cradle
