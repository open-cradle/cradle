#ifndef CRADLE_TESTS_INNER_DLL_V1_ADDER_V1_H
#define CRADLE_TESTS_INNER_DLL_V1_ADDER_V1_H

#include <string>

#include <cradle/inner/requests/function.h>

namespace cradle {

inline auto
rq_test_adder_v1(int a, int b)
{
    constexpr bool is_proxy{true};
    using props_type
        = request_props<caching_level_type::none, false, false, is_proxy>;
    request_uuid uuid{"test-adder-v1"};
    return rq_function_erased<int>(props_type{std::move(uuid)}, a, b);
}

} // namespace cradle

#endif
