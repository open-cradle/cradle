#include <boost/dll.hpp>

#include "thinknode_seri_v1.h"

namespace cradle {

extern "C" BOOST_SYMBOL_EXPORT selfreg_seri_catalog*
CRADLE_create_seri_catalog()
{
    return new thinknode_seri_catalog_v1;
}

} // namespace cradle
