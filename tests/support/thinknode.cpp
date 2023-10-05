#include <stdexcept>

#include <fmt/format.h>

#include "inner_service.h"
#include "outer_service.h"
#include "thinknode.h"
#include <cradle/inner/dll/shared_library.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache_plugin.h>
#include <cradle/rpclib/client/registry.h>
#include <cradle/thinknode/domain_factory.h>
#include <cradle/thinknode_dlls_dir.h>

namespace cradle {

thinknode_test_scope::thinknode_test_scope(std::string const& proxy_name)
    : proxy_name_{proxy_name}
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
            register_rpclib_client(make_outer_tests_config(), resources_);
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
    // TODO outer config?
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
    session.access_token = "xyz";
    return thinknode_request_context{
        resources_, session, tasklet, proxy_ != nullptr, proxy_name_};
}

} // namespace cradle
