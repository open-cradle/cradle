#include <memory>

#include <boost/dll.hpp>

#include "thinknode_seri_v1.h"
#include <cradle/inner/dll/dll_capabilities.h>

namespace cradle {

static std::unique_ptr<selfreg_seri_catalog>
create_my_catalog(seri_registry& registry)
{
    return std::make_unique<thinknode_seri_catalog_v1>(registry);
}

static constexpr dll_capabilities my_capabilities{
    .create_seri_catalog = create_my_catalog};

extern "C" BOOST_SYMBOL_EXPORT dll_capabilities const*
CRADLE_get_capabilities()
{
    return &my_capabilities;
}

} // namespace cradle
