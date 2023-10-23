#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

class test_seri_catalog_x1 : public selfreg_seri_catalog
{
 public:
    test_seri_catalog_x1(std::shared_ptr<seri_registry> registry);
};

} // namespace cradle
