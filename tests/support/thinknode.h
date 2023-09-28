#ifndef CRADLE_TESTS_SUPPORT_THINKNODE_H
#define CRADLE_TESTS_SUPPORT_THINKNODE_H

#include <memory>
#include <string>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/thinknode/context.h>
#include <cradle/thinknode/service/core.h>

namespace cradle {

void
ensure_all_domains_registered();

// Existence of an object of this class makes it possible to create,
// deserialize and resolve Thinknode requests via the local or remote
// service identified by proxy_name.
// Proxy request objects can still be created if no scope object exists.
// TODO make it impossible to create Thinknode requests outside scope
class thinknode_test_scope
{
 public:
    // proxy_name should be "" (local), "loopback" or "rpclib"
    thinknode_test_scope(std::string const& proxy_name);

    ~thinknode_test_scope();

    service_core&
    get_resources()
    {
        return resources_;
    }

    std::string
    get_proxy_name() const
    {
        return proxy_name_;
    }

    // Returns nullptr for local operation
    remote_proxy*
    get_proxy() const
    {
        return proxy_;
    }

    rpclib_client&
    get_rpclib_client() const;

    thinknode_request_context
    make_context(tasklet_tracker* tasklet = nullptr);

 private:
    static inline const std::string dll_name_{"cradle_thinknode_v1"};

    std::string proxy_name_;
    service_core resources_;
    remote_proxy* proxy_{nullptr};

    void
    register_remote();
};

} // namespace cradle

#endif
