#ifndef CRADLE_INNER_DLL_DLL_EXCEPTIONS_H
#define CRADLE_INNER_DLL_DLL_EXCEPTIONS_H

#include <stdexcept>

namespace cradle {

class dll_load_error : public std::runtime_error
{
 public:
    using std::runtime_error::runtime_error;
};

class dll_unload_error : public std::runtime_error
{
 public:
    using std::runtime_error::runtime_error;
};

} // namespace cradle

#endif
