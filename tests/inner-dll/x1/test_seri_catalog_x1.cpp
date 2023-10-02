#include <boost/dll.hpp>

#include "../x1/multiplier_x1_impl.h"
#include "test_seri_catalog_x1.h"

namespace cradle {

void
test_seri_catalog_x1::try_register_all()
{
    register_resolver(rq_test_multiplier_x1_impl(2, 3));
}

extern "C" BOOST_SYMBOL_EXPORT selfreg_seri_catalog*
CRADLE_create_seri_catalog()
{
    return new test_seri_catalog_x1;
}

} // namespace cradle
