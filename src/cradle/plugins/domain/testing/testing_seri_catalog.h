#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_TESTING_SERI_CATALOG_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_TESTING_SERI_CATALOG_H

#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

class testing_seri_catalog : public selfreg_seri_catalog
{
 private:
    void
    try_register_all() override;
};

} // namespace cradle

#endif
