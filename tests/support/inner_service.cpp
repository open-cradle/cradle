#include "inner_service.h"
#include <cradle/deploy_dir.h>
#include <cradle/inner/blob_file/blob_file_dir.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/introspection/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/domain_factory.h>
#include <cradle/plugins/requests_storage/http/http_requests_storage.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>

namespace cradle {

namespace {

std::string tests_cache_dir{"tests_cache"};
service_config_map const inner_config_map{
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
    {local_disk_cache_config_keys::POLL_INTERVAL, 20U},
    {blob_cache_config_keys::DIRECTORY, tests_cache_dir},
    {http_requests_storage_config_keys::PORT, 9092U},
    {introspection_config_keys::FORCE_FINISH, true},
};

static std::string const the_domain_name{"unit tests"};

} // namespace

service_config
make_inner_tests_config()
{
    return service_config{inner_config_map};
}

std::unique_ptr<inner_resources>
make_inner_test_resources(
    std::string const& proxy_name, domain_option const& domain)
{
    auto config{make_inner_tests_config()};
    auto resources{std::make_unique<inner_resources>(config)};
    resources->set_secondary_cache(std::make_unique<local_disk_cache>(config));
    init_and_register_proxy(*resources, proxy_name, domain);
    if (!proxy_name.empty())
    {
        // Improve test isolation if testing with a long-running rpclib server
        // instance.
        auto& proxy{resources->get_proxy(proxy_name)};
        proxy.clear_unused_mem_cache_entries();
    }
    return resources;
}

non_caching_request_resolution_context::non_caching_request_resolution_context(
    inner_resources& resources)
    : resources_{resources}
{
}

std::string const&
non_caching_request_resolution_context::domain_name() const
{
    return the_domain_name;
}

cppcoro::task<>
non_caching_request_resolution_context::schedule_after(
    std::chrono::milliseconds delay)
{
    auto& io_svc{resources_.the_io_service()};
    co_await io_svc.schedule_after(delay);
}

std::shared_ptr<data_owner>
non_caching_request_resolution_context::make_data_owner(
    std::size_t size, bool use_shared_memory)
{
    if (use_shared_memory)
    {
        throw not_implemented_error();
    }
    return make_shared_buffer(size);
}

caching_request_resolution_context::caching_request_resolution_context(
    inner_resources& resources)
    : resources_{resources}
{
}

std::string const&
caching_request_resolution_context::domain_name() const
{
    return the_domain_name;
}

cppcoro::task<>
caching_request_resolution_context::schedule_after(
    std::chrono::milliseconds delay)
{
    auto& io_svc{resources_.the_io_service()};
    co_await io_svc.schedule_after(delay);
}

std::shared_ptr<data_owner>
caching_request_resolution_context::make_data_owner(
    std::size_t size, bool use_shared_memory)
{
    if (use_shared_memory)
    {
        throw not_implemented_error();
    }
    return make_shared_buffer(size);
}

} // namespace cradle
