#include <memory>
#include <utility>

#include <boost/dll.hpp>

#include "adder_v1_impl.h"
#include "test_seri_catalog_v1.h"
#include <cradle/inner/dll/dll_capabilities.h>

namespace cradle {

test_seri_catalog_v1::test_seri_catalog_v1(
    std::shared_ptr<seri_registry> registry)
    : selfreg_seri_catalog{std::move(registry)}
{
    register_resolver(rq_test_adder_v1p_impl(2, 3));
    register_resolver(rq_test_adder_v1n_impl(2, 3));
}

static std::unique_ptr<selfreg_seri_catalog>
create_my_catalog(std::shared_ptr<seri_registry> registry)
{
    return std::make_unique<test_seri_catalog_v1>(std::move(registry));
}

static constexpr dll_capabilities my_capabilities{
    .create_seri_catalog = create_my_catalog};

extern "C" BOOST_SYMBOL_EXPORT dll_capabilities const*
CRADLE_get_capabilities()
{
    return &my_capabilities;
}

} // namespace cradle
