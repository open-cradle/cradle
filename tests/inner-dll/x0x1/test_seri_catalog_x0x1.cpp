#include <memory>

#include <boost/dll.hpp>

#include "../x0/adder_x0_impl.h"
#include "../x1/multiplier_x1_impl.h"
#include "test_seri_catalog_x0x1.h"
#include <cradle/inner/dll/dll_capabilities.h>

namespace cradle {

void
test_seri_catalog_x0x1::try_register_all()
{
    register_resolver(rq_test_adder_x0_impl(2, 3));
    register_resolver(rq_test_multiplier_x1_impl(2, 3));
}

static std::unique_ptr<selfreg_seri_catalog>
create_my_catalog(seri_registry const& registry)
{
    return std::make_unique<test_seri_catalog_x0x1>();
}

static constexpr dll_capabilities my_capabilities{
    .create_seri_catalog = create_my_catalog};

extern "C" BOOST_SYMBOL_EXPORT dll_capabilities const*
CRADLE_get_capabilities()
{
    return &my_capabilities;
}

} // namespace cradle
