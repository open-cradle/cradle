#include <memory>

#include <boost/dll.hpp>

#include <cradle/inner/dll/dll_capabilities.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>

namespace cradle {

static std::unique_ptr<selfreg_seri_catalog>
create_my_catalog(std::shared_ptr<seri_registry> registry)
{
    return nullptr;
}

static constexpr dll_capabilities my_capabilities{
    .create_seri_catalog = create_my_catalog};

extern "C" BOOST_SYMBOL_EXPORT dll_capabilities const*
CRADLE_get_capabilities()
{
    return &my_capabilities;
}

} // namespace cradle
