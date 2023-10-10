#include "inner_service.h"
#include <cradle/deploy_dir.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/domain_factory.h>
#include <cradle/plugins/requests_storage/http/http_requests_storage.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
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
    {local_disk_cache_config_keys::START_EMPTY, true},
    {blob_cache_config_keys::DIRECTORY, tests_cache_dir},
    {http_requests_storage_config_keys::PORT, 9092U},
};

service_config
make_inner_tests_config()
{
    return service_config{inner_config_map};
}

inner_resources
make_inner_test_resources()
{
    auto config{make_inner_tests_config()};
    inner_resources resources{config};
    resources.set_secondary_cache(std::make_unique<local_disk_cache>(config));
    return resources;
}

static std::string loopback_cache_dir{"loopback_cache"};
static service_config_map const loopback_config_map{
    {generic_config_keys::TESTING, true},
    {generic_config_keys::DEPLOY_DIR, get_deploy_dir()},
    {inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00U},
    {inner_config_keys::SECONDARY_CACHE_FACTORY,
     local_disk_cache_config_values::PLUGIN_NAME},
    {local_disk_cache_config_keys::DIRECTORY, loopback_cache_dir},
    {local_disk_cache_config_keys::SIZE_LIMIT, 0x40'00'00'00U},
    {local_disk_cache_config_keys::NUM_THREADS_READ_POOL, 2U},
    {local_disk_cache_config_keys::NUM_THREADS_WRITE_POOL, 2U},
    {local_disk_cache_config_keys::START_EMPTY, true},
    {blob_cache_config_keys::DIRECTORY, loopback_cache_dir},
    {http_requests_storage_config_keys::PORT, 9092U},
};

service_config
make_inner_loopback_config()
{
    return service_config{loopback_config_map};
}

void
init_test_loopback_service(
    inner_resources& test_resources, bool with_testing_domain)
{
    service_config loopback_config{make_inner_loopback_config()};
    auto loopback_resources{
        std::make_unique<inner_resources>(loopback_config)};
    loopback_resources->set_secondary_cache(
        std::make_unique<local_disk_cache>(loopback_config));
    if (with_testing_domain)
    {
        loopback_resources->register_domain(
            create_testing_domain(*loopback_resources));
    }
    test_resources.register_proxy(std::make_unique<loopback_service>(
        loopback_config, std::move(loopback_resources)));
}

caching_request_resolution_context::caching_request_resolution_context()
    : resources{make_inner_test_resources()}
{
}

void
caching_request_resolution_context::reset_memory_cache()
{
    resources.reset_memory_cache();
}

} // namespace cradle
