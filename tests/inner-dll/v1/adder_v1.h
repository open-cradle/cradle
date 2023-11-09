#ifndef CRADLE_TESTS_INNER_DLL_V1_ADDER_V1_H
#define CRADLE_TESTS_INNER_DLL_V1_ADDER_V1_H

#include <string>

#include "adder_v1_defs.h"
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

// Plain args.
inline auto
rq_test_adder_v1p(int a, int b)
{
    using props_type = adder_v1_props<true>;
    return rq_function_erased<int>(
        props_type{request_uuid{adder_v1p_uuid}, adder_v1_title}, a, b);
}

// Normalized args.
template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_adder_v1n(A a, B b)
{
    using props_type = adder_v1_props<true>;
    return rq_function_erased<int>(
        props_type{request_uuid{adder_v1n_uuid}, adder_v1_title},
        normalize_arg<int, props_type>(std::move(a)),
        normalize_arg<int, props_type>(std::move(b)));
}

} // namespace cradle

#endif
