#include <boost/dll.hpp>

#include "thinknode_seri_v1.h"

namespace cradle {

static thinknode_seri_catalog_v1 the_catalog{false};

// To be called when the DLL is loaded
extern "C" BOOST_SYMBOL_EXPORT void
CRADLE_init()
{
    the_catalog.register_all();
}

extern "C" BOOST_SYMBOL_EXPORT seri_catalog*
CRADLE_get_catalog()
{
    return &the_catalog;
}

} // namespace cradle
