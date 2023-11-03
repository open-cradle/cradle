#ifndef CRADLE_TESTS_INNER_DLL_X0_ADDER_X0_H
#define CRADLE_TESTS_INNER_DLL_X0_ADDER_X0_H

#include <memory>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

inline auto
rq_test_adder_x0(int a, int b)
{
    constexpr bool is_proxy{true};
    using props_type
        = request_props<caching_level_type::none, false, false, is_proxy>;
    request_uuid uuid{"test-adder-x0"};
    return rq_function_erased<int>(props_type{std::move(uuid)}, a, b);
}

} // namespace cradle

#endif
