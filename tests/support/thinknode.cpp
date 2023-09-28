#include <stdexcept>

#include <fmt/format.h>

#include "inner_service.h"
#include "outer_service.h"
#include "thinknode.h"
#include <cradle/inner/dll/shared_library.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/plugins/domain/all/all_domains.h>
#include <cradle/rpclib/client/registry.h>
#include <cradle/thinknode_dlls_dir.h>

namespace cradle {

void
ensure_all_domains_registered()
{
    static bool registered{false};
    if (!registered)
    {
        // TODO unregister when done; part of resources?
        register_and_initialize_all_domains();
        registered = true;
    }
}

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
            // TODO unregister remote service in dtor
            register_loopback_service(make_inner_tests_config(), resources_);
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

rpclib_client&
thinknode_test_scope::get_rpclib_client() const
{
    if (proxy_name_ != "rpclib")
    {
        throw std::logic_error(
            fmt::format("No rpc client for proxy {}", proxy_name_));
    }
    return static_cast<rpclib_client&>(*proxy_);
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
