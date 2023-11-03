#ifndef CRADLE_TESTS_INNER_DLL_X1_MULTIPLIER_X1_IMPL_H
#define CRADLE_TESTS_INNER_DLL_X1_MULTIPLIER_X1_IMPL_H

#include <memory>
#include <string>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

int
multiplier_x1_func(int a, int b);

inline auto
rq_test_multiplier_x1_impl(int a, int b)
{
    constexpr bool is_proxy{false};
    using props_type
        = request_props<caching_level_type::none, false, false, is_proxy>;
    request_uuid uuid{"test-multiplier-x1"};
    std::string title{"test-multiplier"};
    return rq_function_erased(
        props_type{std::move(uuid), std::move(title)},
        multiplier_x1_func,
        a,
        b);
}

} // namespace cradle

#endif
