#include <memory>
#include <utility>

#include <boost/dll.hpp>

#include "m0_meta_impl.h"
#include "m0_meta_seri_catalog.h"
#include <cradle/inner/dll/dll_capabilities.h>

namespace cradle {

m0_meta_seri_catalog::m0_meta_seri_catalog(
    std::shared_ptr<seri_registry> registry)
    : selfreg_seri_catalog{std::move(registry)}
{
    register_resolver(rq_test_m0_metap_impl(0, 0));
    register_resolver(rq_test_m0_metan_impl(0, 0));
    register_resolver(rq_test_m0_metavecp_impl(std::vector<int>{}));
    register_resolver(m0_make_inner_request_func(0, 0));
}

static std::unique_ptr<selfreg_seri_catalog>
create_my_catalog(std::shared_ptr<seri_registry> registry)
{
    return std::make_unique<m0_meta_seri_catalog>(std::move(registry));
}

static constexpr dll_capabilities my_capabilities{
    .create_seri_catalog = create_my_catalog};

extern "C" BOOST_SYMBOL_EXPORT dll_capabilities const*
CRADLE_get_capabilities()
{
    return &my_capabilities;
}

} // namespace cradle
