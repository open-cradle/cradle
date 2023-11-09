#include <memory>
#include <utility>

#include <boost/dll.hpp>

#include "seri_catalog_t0.h"
#include <cradle/inner/dll/dll_capabilities.h>

namespace cradle {

static std::unique_ptr<selfreg_seri_catalog>
create_my_catalog(std::shared_ptr<seri_registry> registry)
{
    return std::make_unique<seri_catalog_t0>(std::move(registry));
}

static constexpr dll_capabilities my_capabilities{
    .create_seri_catalog = create_my_catalog};

extern "C" BOOST_SYMBOL_EXPORT dll_capabilities const*
CRADLE_get_capabilities()
{
    return &my_capabilities;
}

} // namespace cradle
