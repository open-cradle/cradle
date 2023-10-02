#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

class test_seri_catalog_x0 : public selfreg_seri_catalog
{
 private:
    void
    try_register_all() override;
};

} // namespace cradle
