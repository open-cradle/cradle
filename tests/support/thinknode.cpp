#include "thinknode.h"
#include <cradle/plugins/domain/all/all_domains.h>
#include <cradle/thinknode/seri_catalog.h>

namespace cradle {

void
ensure_thinknode_seri_resolvers()
{
    static bool registered{false};
    if (!registered)
    {
        register_thinknode_seri_resolvers();
        registered = true;
    }
}

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

} // namespace cradle
