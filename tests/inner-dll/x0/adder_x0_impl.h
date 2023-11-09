#ifndef CRADLE_TESTS_INNER_DLL_X0_ADDER_X0_IMPL_H
#define CRADLE_TESTS_INNER_DLL_X0_ADDER_X0_IMPL_H

#include "adder_x0_defs.h"
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

int
adder_x0_func(int a, int b);

inline auto
rq_test_adder_x0_impl(int a, int b)
{
    using props_type = adder_x0_props<false>;
    return rq_function_erased(
        props_type{request_uuid{adder_x0_uuid}}, adder_x0_func, a, b);
}

} // namespace cradle

#endif
