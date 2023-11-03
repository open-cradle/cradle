#include <boost/dll.hpp>

#include <cradle/inner/dll/dll_capabilities.h>

namespace cradle {

extern "C" BOOST_SYMBOL_EXPORT dll_capabilities const*
CRADLE_get_capabilities_this_is_the_wrong_name()
{
    return nullptr;
}

} // namespace cradle
