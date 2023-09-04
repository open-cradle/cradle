#ifndef CRADLE_TESTS_INNER_DLL_X0_ADDER_X0_IMPL_H
#define CRADLE_TESTS_INNER_DLL_X0_ADDER_X0_IMPL_H

#include <memory>
#include <string>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

int
adder_x0_func(int a, int b);

inline auto
rq_test_adder_x0_impl(int a, int b)
{
    constexpr bool is_proxy{false};
    using props_type
        = request_props<caching_level_type::none, false, false, is_proxy>;
    request_uuid uuid{"test-adder-x0"};
    std::string title{"test-adder"};
    return rq_function_erased(
        props_type{std::move(uuid), std::move(title)}, adder_x0_func, a, b);
}

} // namespace cradle

#endif
