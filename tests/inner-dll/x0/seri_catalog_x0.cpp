#include <boost/dll.hpp>

#include "adder_x0_impl.h"
#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

static seri_catalog&
get_catalog()
{
    static seri_catalog the_catalog;
    return the_catalog;
}

// To be called when the DLL is loaded
extern "C" BOOST_SYMBOL_EXPORT void
CRADLE_init()
{
    auto& cat{get_catalog()};
    cat.register_resolver(rq_test_adder_x0_impl(2, 3));
}

extern "C" BOOST_SYMBOL_EXPORT seri_catalog*
CRADLE_get_catalog()
{
    return &get_catalog();
}

} // namespace cradle
