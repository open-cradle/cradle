#include <stdexcept>

#include <fmt/format.h>

#include "common.h"
#include <cradle/deploy_dir.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/domain_factory.h>
#include <cradle/plugins/requests_storage/http/http_requests_storage.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

namespace {

void
init_and_register_local(
    inner_resources& resources, domain_option const& domain)
{
    domain.register_domain(resources);
}

std::string loopback_cache_dir{"loopback_cache"};
service_config_map const loopback_config_map{
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
init_and_register_loopback(
    inner_resources& test_resources, domain_option const& domain)
{
    service_config loopback_config{make_inner_loopback_config()};
    auto loopback_resources{
        std::make_unique<inner_resources>(loopback_config)};
    loopback_resources->set_secondary_cache(
        std::make_unique<local_disk_cache>(loopback_config));
    domain.register_domain(*loopback_resources);
    test_resources.register_proxy(std::make_unique<loopback_service>(
        loopback_config, std::move(loopback_resources)));
}

void
init_and_register_rpclib(
    inner_resources& resources, domain_option const& domain)
{
    resources.register_proxy(
        std::make_unique<rpclib_client>(resources.config()));
}

} // namespace

void
no_domain_option::register_domain(inner_resources& resources) const
{
}

void
testing_domain_option::register_domain(inner_resources& resources) const
{
    resources.register_domain(create_testing_domain(resources));
}

void
init_and_register_proxy(
    inner_resources& resources,
    std::string const& proxy_name,
    domain_option const& domain)
{
    if (proxy_name == "")
    {
        init_and_register_local(resources, domain);
    }
    else if (proxy_name == "loopback")
    {
        init_and_register_loopback(resources, domain);
    }
    else if (proxy_name == "rpclib")
    {
        init_and_register_rpclib(resources, domain);
    }
    else
    {
        throw std::domain_error{
            fmt::format("invalid proxy name {}", proxy_name)};
    }
}

} // namespace cradle
