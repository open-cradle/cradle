#include <memory>

#include <boost/dll.hpp>

#include <cradle/inner/dll/dll_capabilities.h>

namespace cradle {

static constexpr dll_capabilities my_capabilities;

extern "C" BOOST_SYMBOL_EXPORT dll_capabilities const*
CRADLE_get_capabilities()
{
    return &my_capabilities;
}

} // namespace cradle
