#ifndef CRADLE_TESTS_INNER_DLL_V1_ADDER_V1_H
#define CRADLE_TESTS_INNER_DLL_V1_ADDER_V1_H

#include <string>

#include <cradle/inner/requests/function.h>

namespace cradle {

// Plain args
inline auto
rq_test_adder_v1p(int a, int b)
{
    constexpr bool is_proxy{true};
    using props_type
        = request_props<caching_level_type::none, false, false, is_proxy>;
    request_uuid uuid{"test-adder-v1p"};
    return rq_function_erased<int>(props_type{std::move(uuid)}, a, b);
}

// Normalized args
template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_adder_v1n(A a, B b)
{
    constexpr bool is_proxy{true};
    using props_type
        = request_props<caching_level_type::none, false, false, is_proxy>;
    request_uuid uuid{"test-adder-v1n"};
    return rq_function_erased<int>(
        props_type{std::move(uuid)},
        normalize_arg<int, props_type>(std::move(a)),
        normalize_arg<int, props_type>(std::move(b)));
}

} // namespace cradle

#endif
