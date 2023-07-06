#ifndef CRADLE_INNER_UTILITIES_DEMANGLE_H
#define CRADLE_INNER_UTILITIES_DEMANGLE_H

#include <string>
#include <typeindex>

namespace cradle {

// Tries to convert a mangled type name to something more comprehensible.
// Returns the original name if the attempt fails.
std::string
demangle(std::type_index key);

} // namespace cradle

#endif
