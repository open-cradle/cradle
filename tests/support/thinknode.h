#ifndef CRADLE_TESTS_SUPPORT_THINKNODE_H
#define CRADLE_TESTS_SUPPORT_THINKNODE_H

#include <memory>
#include <string>

#include "common.h"
#include <cradle/inner/dll/dll_collection.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/thinknode/async_context.h>
#include <cradle/thinknode/context.h>
#include <cradle/thinknode/service/core.h>

namespace cradle {

// Existence of an object of this class makes it possible to create,
// deserialize and resolve Thinknode requests via the local or remote
// service identified by proxy_name.
// Proxy request objects can still be created if no scope object exists.
// TODO make it impossible to create Thinknode requests outside scope
class thinknode_test_scope
{
 public:
    // proxy_name should be "" (local, default), "loopback" or "rpclib"
    thinknode_test_scope(
        std::string const& proxy_name = {}, bool use_real_api_token = false);

    ~thinknode_test_scope();

    service_core&
    get_resources()
    {
        return *resources_;
    }

    // Returns nullptr for local operation
    remote_proxy*
    get_proxy() const
    {
        return proxy_;
    }

    thinknode_request_context
    make_context(tasklet_tracker* tasklet = nullptr);

    async_thinknode_context
    make_async_context(tasklet_tracker* tasklet = nullptr);

    mock_http_session&
    enable_http_mocking();

    void
    clear_caches();

 private:
    static inline const std::string dll_name_{"cradle_thinknode_v1"};

    std::string proxy_name_;
    bool use_real_api_token_;
    std::unique_ptr<service_core> resources_;
    remote_proxy* proxy_{nullptr};
};

// Create resources for Thinknode testing purposes;
// - optionally registering a single remote proxy; and
// - optionally adding a single domain.
// proxy_name should be "" (local, default), "loopback" or "rpclib"
std::unique_ptr<service_core>
make_thinknode_test_resources(
    std::string const& proxy_name = {},
    domain_option const& domain = no_domain_option());

} // namespace cradle

#endif
