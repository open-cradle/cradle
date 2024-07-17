#ifndef CRADLE_TESTS_INNER_DLL_M0_META_M0_META_SERI_CATALOG_H
#define CRADLE_TESTS_INNER_DLL_M0_META_M0_META_SERI_CATALOG_H

#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

class m0_meta_seri_catalog : public selfreg_seri_catalog
{
 public:
    m0_meta_seri_catalog(std::shared_ptr<seri_registry> registry);
};

} // namespace cradle

#endif
