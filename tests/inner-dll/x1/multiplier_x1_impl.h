#ifndef CRADLE_TESTS_INNER_DLL_X1_MULTIPLIER_X1_IMPL_H
#define CRADLE_TESTS_INNER_DLL_X1_MULTIPLIER_X1_IMPL_H

#include "multiplier_x1_defs.h"
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

int
multiplier_x1_func(int a, int b);

inline auto
rq_test_multiplier_x1_impl(int a, int b)
{
    using props_type = multiplier_x1_props<false>;
    return rq_function(
        props_type{request_uuid{multiplier_x1_uuid}},
        multiplier_x1_func,
        a,
        b);
}

} // namespace cradle

#endif
