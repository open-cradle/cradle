#include <stdexcept>

#include <fmt/format.h>

#include "inner_service.h"
#include "thinknode.h"
#include <cradle/deploy_dir.h>
#include <cradle/inner/dll/shared_library.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/utilities/environment.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/rpclib/client/registry.h>
#include <cradle/thinknode/domain_factory.h>
#include <cradle/thinknode_dlls_dir.h>

namespace cradle {

namespace {

std::string tests_cache_dir{"tests_cache"};
service_config_map const thinknode_config_map{
    {generic_config_keys::TESTING, true},
    {generic_config_keys::DEPLOY_DIR, get_deploy_dir()},
    {inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00U},
    {inner_config_keys::SECONDARY_CACHE_FACTORY,
     local_disk_cache_config_values::PLUGIN_NAME},
    {local_disk_cache_config_keys::DIRECTORY, tests_cache_dir},
    {local_disk_cache_config_keys::SIZE_LIMIT, 0x40'00'00'00U},
    {local_disk_cache_config_keys::START_EMPTY, true},
    {inner_config_keys::HTTP_CONCURRENCY, 2U}};

service_config
make_thinknode_tests_config()
{
    return service_config{thinknode_config_map};
}

class thinknode_domain_option : public domain_option
{
 public:
    void
    register_domain(inner_resources& resources) const override
    {
        resources.register_domain(
            create_thinknode_domain(static_cast<service_core&>(resources)));
    }
};

} // namespace

thinknode_test_scope::thinknode_test_scope(
    std::string const& proxy_name, bool use_real_api_token)
    : proxy_name_{proxy_name},
      use_real_api_token_{use_real_api_token},
      resources_{
          make_thinknode_test_resources(proxy_name, thinknode_domain_option{})}
{
    if (!proxy_name_.empty())
    {
        proxy_ = &resources_.get_proxy(proxy_name_);
    }
    if (proxy_name_ == "rpclib")
    {
        // Maybe cleaner to do this for loopback too.
        proxy_->load_shared_library(get_thinknode_dlls_dir(), dll_name_);
    }
    else
    {
        load_shared_library(get_thinknode_dlls_dir(), dll_name_);
    }
}

thinknode_test_scope::~thinknode_test_scope()
{
    if (proxy_name_ == "rpclib")
    {
        proxy_->unload_shared_library(dll_name_);
    }
    else
    {
        unload_shared_library(dll_name_);
    }
}

thinknode_request_context
thinknode_test_scope::make_context(tasklet_tracker* tasklet)
{
    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token
        = use_real_api_token_
              ? get_environment_variable("CRADLE_THINKNODE_API_TOKEN")
              : "xyz";
    auto proxy_name{get_proxy_name()};
    return thinknode_request_context{resources_, session, tasklet, proxy_name};
}

mock_http_session&
thinknode_test_scope::enable_http_mocking()
{
    return ::cradle::enable_http_mocking(resources_);
}

void
thinknode_test_scope::clear_caches()
{
    // TODO clear remote cache for rpclib?
    resources_.reset_memory_cache();
    resources_.clear_secondary_cache();
}

service_core
make_thinknode_test_resources(
    std::string const& proxy_name, domain_option const& domain)
{
    auto config{make_thinknode_tests_config()};
    service_core resources{config};
    resources.set_secondary_cache(std::make_unique<local_disk_cache>(config));
    init_and_register_proxy(resources, proxy_name, domain);
    return resources;
}

} // namespace cradle
