#include <stdexcept>

#include <fmt/format.h>

#include "inner_service.h"
#include "thinknode.h"
#include <cradle/deploy_dir.h>
#include <cradle/inner/dll/shared_library.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/utilities/environment.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache_plugin.h>
#include <cradle/rpclib/client/registry.h>
#include <cradle/thinknode/domain_factory.h>
#include <cradle/thinknode_dlls_dir.h>

namespace cradle {

static std::string tests_cache_dir{"tests_cache"};
static service_config_map const thinknode_config_map{
    {generic_config_keys::TESTING, true},
    {generic_config_keys::DEPLOY_DIR, get_deploy_dir()},
    {inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00U},
    {inner_config_keys::SECONDARY_CACHE_FACTORY,
     local_disk_cache_config_values::PLUGIN_NAME},
    {local_disk_cache_config_keys::DIRECTORY, tests_cache_dir},
    {local_disk_cache_config_keys::SIZE_LIMIT, 0x40'00'00'00U},
    {local_disk_cache_config_keys::START_EMPTY, true},
    {inner_config_keys::HTTP_CONCURRENCY, 2U}};

static service_config
make_thinknode_tests_config()
{
    return service_config{thinknode_config_map};
}

thinknode_test_scope::thinknode_test_scope(
    std::string const& proxy_name, bool use_real_api_token)
    : proxy_name_{proxy_name}, use_real_api_token_{use_real_api_token}
{
    init_test_service(resources_);
    register_remote();
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

void
thinknode_test_scope::register_remote()
{
    if (!proxy_name_.empty())
    {
        if (proxy_name_ == "loopback")
        {
            init_loopback_service();
        }
        else if (proxy_name_ == "rpclib")
        {
            register_rpclib_client(make_thinknode_tests_config(), resources_);
        }
        else
        {
            throw std::invalid_argument(
                fmt::format("Unknown proxy name {}", proxy_name_));
        }
        proxy_ = &resources_.get_proxy(proxy_name_);
    }
}

void
thinknode_test_scope::init_loopback_service()
{
    // TODO thinknode config?
    service_config loopback_config{make_inner_loopback_config()};
    auto loopback_resources{std::make_unique<service_core>()};
    activate_local_disk_cache_plugin(*loopback_resources);
    loopback_resources->initialize(loopback_config);
    loopback_resources->register_domain(
        create_thinknode_domain(*loopback_resources));
    resources_.register_proxy(std::make_unique<loopback_service>(
        loopback_config, std::move(loopback_resources)));
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
    return thinknode_request_context{
        resources_, session, tasklet, proxy_ != nullptr, proxy_name_};
}

mock_http_session&
thinknode_test_scope::enable_http_mocking()
{
    return ::cradle::enable_http_mocking(resources_);
}

void
thinknode_test_scope::clear_caches()
{
    auto config{make_thinknode_tests_config()};
    // TODO clear remote cache for rpclib?
    resources_.reset_memory_cache(config);
    resources_.clear_secondary_cache();
}

void
init_test_service(service_core& core)
{
    activate_local_disk_cache_plugin(core);
    core.initialize(make_thinknode_tests_config());
}

} // namespace cradle
