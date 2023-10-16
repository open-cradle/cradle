#include <boost/dll.hpp>

#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

extern "C" BOOST_SYMBOL_EXPORT selfreg_seri_catalog*
CRADLE_create_seri_catalog_this_is_the_wrong_name()
{
    return nullptr;
}

} // namespace cradle
