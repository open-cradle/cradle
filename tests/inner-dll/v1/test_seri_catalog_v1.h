#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

class test_seri_catalog_v1 : public selfreg_seri_catalog
{
 public:
    test_seri_catalog_v1(seri_registry& registry);
};

} // namespace cradle
