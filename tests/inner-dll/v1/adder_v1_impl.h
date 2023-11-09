#ifndef CRADLE_TESTS_INNER_DLL_ADDER_V1_ADDER_V1_IMPL_H
#define CRADLE_TESTS_INNER_DLL_ADDER_V1_ADDER_V1_IMPL_H

#include "adder_v1_defs.h"
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

int
adder_v1_func(int a, int b);

inline auto
rq_test_adder_v1p_impl(int a, int b)
{
    using props_type = adder_v1_props<false>;
    return rq_function_erased(
        props_type{request_uuid{adder_v1p_uuid}, adder_v1_title},
        adder_v1_func,
        a,
        b);
}

template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_adder_v1n_impl(A a, B b)
{
    using props_type = adder_v1_props<false>;
    return rq_function_erased(
        props_type{request_uuid{adder_v1n_uuid}, adder_v1_title},
        adder_v1_func,
        normalize_arg<int, props_type>(std::move(a)),
        normalize_arg<int, props_type>(std::move(b)));
}

} // namespace cradle

#endif
