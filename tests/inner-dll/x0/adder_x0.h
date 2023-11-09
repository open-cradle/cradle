#ifndef CRADLE_TESTS_INNER_DLL_X0_ADDER_X0_H
#define CRADLE_TESTS_INNER_DLL_X0_ADDER_X0_H

#include "adder_x0_defs.h"
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

inline auto
rq_test_adder_x0(int a, int b)
{
    using props_type = adder_x0_props<true>;
    return rq_function_erased<int>(
        props_type{request_uuid{adder_x0_uuid}}, a, b);
}

} // namespace cradle

#endif
