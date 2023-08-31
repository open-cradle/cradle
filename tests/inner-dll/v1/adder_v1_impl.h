#ifndef CRADLE_TESTS_INNER_DLL_ADDER_V1_ADDER_V1_IMPL_H
#define CRADLE_TESTS_INNER_DLL_ADDER_V1_ADDER_V1_IMPL_H

#include <string>

#include <cradle/inner/requests/function.h>

namespace cradle {

int
adder_v1_func(int a, int b);

inline auto
rq_test_adder_v1_impl(int a, int b)
{
    constexpr bool is_proxy{false};
    using props_type
        = request_props<caching_level_type::none, false, false, is_proxy>;
    request_uuid uuid{"test-adder-v1"};
    std::string title{"test-adder"};
    return rq_function_erased(
        props_type{std::move(uuid), std::move(title)}, adder_v1_func, a, b);
}

} // namespace cradle

#endif
