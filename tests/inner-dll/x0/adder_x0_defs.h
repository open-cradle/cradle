#ifndef CRADLE_TESTS_INNER_DLL_ADDER_X0_ADDER_X0_DEFS_H
#define CRADLE_TESTS_INNER_DLL_ADDER_X0_ADDER_X0_DEFS_H

#include <string>

#include <cradle/inner/requests/request_props.h>

namespace cradle {

template<bool is_proxy>
using adder_x0_props = request_props<
    caching_level_type::none,
    is_proxy ? request_function_t::proxy_plain : request_function_t::plain,
    false>;

static inline std::string const adder_x0_uuid{"test-adder-x0"};

} // namespace cradle

#endif
