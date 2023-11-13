#ifndef CRADLE_TESTS_INNER_DLL_ADDER_V1_ADDER_V1_DEFS_H
#define CRADLE_TESTS_INNER_DLL_ADDER_V1_ADDER_V1_DEFS_H

#include <string>

#include <cradle/inner/requests/request_props.h>

namespace cradle {

template<bool is_proxy>
using adder_v1_props = request_props<
    caching_level_type::none,
    is_proxy ? request_function_t::proxy_plain : request_function_t::plain,
    true>;

static inline std::string const adder_v1p_uuid{"test-adder-v1p"};
static inline std::string const adder_v1n_uuid{"test-adder-v1n"};
static inline std::string const adder_v1_title{"test-adder"};

} // namespace cradle

#endif
