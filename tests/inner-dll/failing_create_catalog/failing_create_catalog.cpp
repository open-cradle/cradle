#include <boost/dll.hpp>

namespace cradle {

class selfreg_seri_catalog;

extern "C" BOOST_SYMBOL_EXPORT selfreg_seri_catalog*
CRADLE_create_seri_catalog()
{
    return nullptr;
}

} // namespace cradle
