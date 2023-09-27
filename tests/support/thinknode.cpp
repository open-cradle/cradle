#include "thinknode.h"
#include <cradle/inner/dll/shared_library.h>
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

thinknode_catalog_scope::thinknode_catalog_scope(remote_proxy* proxy)
    : proxy_{proxy}
{
    if (proxy_ && proxy_->name() == "rpclib")
    {
        // Maybe cleaner to do this for loopback too.
        proxy_->load_shared_library(
            get_thinknode_dlls_dir(), "cradle_thinknode_v1");
    }
    else
    {
        load_shared_library(get_thinknode_dlls_dir(), "cradle_thinknode_v1");
    }
}

thinknode_catalog_scope::~thinknode_catalog_scope()
{
    if (proxy_ && proxy_->name() == "rpclib")
    {
        proxy_->unload_shared_library("cradle_thinknode_v1");
    }
    else
    {
        unload_shared_library("cradle_thinknode_v1");
    }
}

} // namespace cradle
