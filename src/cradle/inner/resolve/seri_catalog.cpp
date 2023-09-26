#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>

namespace cradle {

seri_catalog::~seri_catalog()
{
    unregister_all();
}

void
seri_catalog::unregister_all() noexcept
{
    // TODO this must not throw
    seri_registry::instance().unregister_catalog(cat_id_);
}

} // namespace cradle
