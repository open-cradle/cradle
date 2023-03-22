#include "thinknode.h"
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

} // namespace cradle
