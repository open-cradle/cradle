#ifndef _MSC_VER
#include <cxxabi.h>
#endif

#include <cradle/inner/utilities/demangle.h>

namespace cradle {

std::string
demangle(std::type_index key)
{
    std::string mangled{key.name()};
#ifndef _MSC_VER
    // This has been seen to fail on lambdas wrapping a C++ function.
    int status{};
    if (char* ptr
        = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status))
    {
        std::string demangled(ptr);
        free(ptr);
        return demangled;
    }
#endif
    return mangled;
}

} // namespace cradle
