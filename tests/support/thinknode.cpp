#include "thinknode.h"
#include <cradle/plugins/domain/all/all_domains.h>
#include <cradle/thinknode_dlls_dir.h>

namespace cradle {

void
ensure_all_domains_registered()
{
    static bool registered{false};
    if (!registered)
    {
        register_and_initialize_all_domains();
        registered = true;
    }
}

thinknode_catalog_scope::thinknode_catalog_scope(rpclib_client* rpc_proxy)
    : rpc_proxy_{rpc_proxy}
{
    if (rpc_proxy_)
    {
        rpc_proxy_->load_shared_library(
            get_thinknode_dlls_dir(), "cradle_thinknode_v1");
    }
    else
    {
        local_cat_.reset(new thinknode_seri_catalog{true});
    }
}

thinknode_catalog_scope::~thinknode_catalog_scope()
{
    if (rpc_proxy_)
    {
        rpc_proxy_->unload_shared_library("cradle_thinknode_v1");
    }
}

} // namespace cradle
