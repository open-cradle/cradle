#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>

namespace cradle {

seri_catalog::~seri_catalog()
{
    seri_registry::instance().unregister_catalog(cat_id_);
}

} // namespace cradle
