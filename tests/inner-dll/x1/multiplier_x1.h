#ifndef CRADLE_TESTS_INNER_DLL_X1_MULTIPLIER_X1_H
#define CRADLE_TESTS_INNER_DLL_X1_MULTIPLIER_X1_H

#include "multiplier_x1_defs.h"
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

inline auto
rq_test_multiplier_x1(int a, int b)
{
    using props_type = multiplier_x1_props<true>;
    return rq_proxy<int>(props_type{request_uuid{multiplier_x1_uuid}}, a, b);
}

} // namespace cradle

#endif
