#ifndef CRADLE_TESTS_INNER_DLL_MULTIPLIER_X1_MULTIPLIER_X1_DEFS_H
#define CRADLE_TESTS_INNER_DLL_MULTIPLIER_X1_MULTIPLIER_X1_DEFS_H

#include <string>

#include <cradle/inner/requests/request_props.h>

namespace cradle {

template<bool is_proxy>
using multiplier_x1_props = request_props<
    caching_level_type::none,
    is_proxy ? request_function_t::proxy_plain : request_function_t::plain,
    false>;

static inline std::string const multiplier_x1_uuid{"test-multiplier-x1"};

} // namespace cradle

#endif
