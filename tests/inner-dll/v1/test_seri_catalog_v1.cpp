#include <boost/dll.hpp>

#include "adder_v1_impl.h"
#include "test_seri_catalog_v1.h"
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>

namespace cradle {

void
test_seri_catalog_v1::try_register_all()
{
    register_resolver(rq_test_adder_v1p_impl(2, 3));
    register_resolver(rq_test_adder_v1n_impl(2, 3));
}

extern "C" BOOST_SYMBOL_EXPORT selfreg_seri_catalog*
CRADLE_create_seri_catalog()
{
    return new test_seri_catalog_v1;
}

} // namespace cradle
