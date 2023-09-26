#ifndef CRADLE_TESTS_SUPPORT_THINKNODE_H
#define CRADLE_TESTS_SUPPORT_THINKNODE_H

#include <memory>

#include <cradle/rpclib/client/proxy.h>
#include <cradle/thinknode/seri_catalog.h>

namespace cradle {

void
ensure_all_domains_registered();

// As long as this object exists, the Thinknode seri resolvers are available
// on the given proxy.
// Proxy is an rpclib_client, or loopback if rpc_proxy is nullptr.
// Currently limited to thinknode_v1.
class thinknode_catalog_scope
{
 public:
    thinknode_catalog_scope(rpclib_client* rpc_proxy);
    ~thinknode_catalog_scope();

 private:
    rpclib_client* rpc_proxy_;

    // Used for loopback only
    std::unique_ptr<thinknode_seri_catalog> local_cat_;
};

} // namespace cradle

#endif
