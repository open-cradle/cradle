#include "inner_service.h"
#include <cradle/deploy_dir.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache_plugin.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/registry.h>

namespace cradle {

static std::string tests_cache_dir{"tests_cache"};
static service_config_map const inner_config_map{
    {generic_config_keys::TESTING, true},
    {generic_config_keys::DEPLOY_DIR, get_deploy_dir()},
    {inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00U},
    {inner_config_keys::SECONDARY_CACHE_FACTORY,
     local_disk_cache_config_values::PLUGIN_NAME},
    {local_disk_cache_config_keys::DIRECTORY, tests_cache_dir},
    {local_disk_cache_config_keys::SIZE_LIMIT, 0x40'00'00'00U},
    {local_disk_cache_config_keys::NUM_THREADS_READ_POOL, 2U},
    {local_disk_cache_config_keys::NUM_THREADS_WRITE_POOL, 2U},
    {blob_cache_config_keys::DIRECTORY, tests_cache_dir},
};

service_config
make_inner_tests_config()
{
    return service_config{inner_config_map};
}

void
init_test_inner_service(inner_resources& resources)
{
    activate_local_disk_cache_plugin();
    reset_directory(file_path(tests_cache_dir));
    resources.inner_initialize(make_inner_tests_config());
}

void
reset_disk_cache(inner_resources& resources)
{
    resources.reset_secondary_cache(make_inner_tests_config());
}

caching_request_resolution_context::caching_request_resolution_context()
{
    init_test_inner_service(resources);
}

void
caching_request_resolution_context::reset_memory_cache()
{
    resources.reset_memory_cache(make_inner_tests_config());
}

void
ensure_loopback_service(inner_resources& resources)
{
    register_loopback_service(make_inner_tests_config(), resources);
}

std::shared_ptr<rpclib_client>
ensure_rpclib_service()
{
    // TODO no static here, add func to get previously registered client
    static std::shared_ptr<rpclib_client> client;
    if (!client)
    {
        client = register_rpclib_client(make_inner_tests_config());
    }
    return client;
}

} // namespace cradle
