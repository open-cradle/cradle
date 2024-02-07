#ifndef CRADLE_INNER_UTILITIES_DEMANGLE_H
#define CRADLE_INNER_UTILITIES_DEMANGLE_H

#include <string>
#include <typeindex>
#include <typeinfo>

namespace cradle {

// Tries to convert a mangled type name to something more comprehensible.
// Returns the original name if the attempt fails.
std::string
demangle(std::type_index key);

inline std::string
demangle(std::type_info const& key)
{
    return demangle(std::type_index{key});
}

template<typename T>
std::string
demangled_type_name()
{
    return demangle(typeid(T));
}

} // namespace cradle

#endif
