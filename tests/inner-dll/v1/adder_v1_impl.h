#ifndef CRADLE_TESTS_INNER_DLL_ADDER_V1_ADDER_V1_IMPL_H
#define CRADLE_TESTS_INNER_DLL_ADDER_V1_ADDER_V1_IMPL_H

#include <cppcoro/task.hpp>

#include "adder_v1_defs.h"
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

class context_intf;
class containment_data;

int
adder_v1_func(int a, int b);

// Creates a non-proxy request, that can be resolved locally or remotely.
// Plain args, disabling the option of subrequests.
inline auto
rq_test_adder_v1p_impl(int a, int b)
{
    using props_type = adder_v1_normal_props;
    return rq_function(
        props_type{request_uuid{adder_v1p_uuid}, adder_v1_title},
        adder_v1_func,
        a,
        b);
}

// Like previous, passing optional containment data
inline auto
rq_test_adder_v1p_impl(containment_data const* containment, int a, int b)
{
    auto req{rq_test_adder_v1p_impl(a, b)};
    if (containment)
    {
        req.set_containment(*containment);
    }
    return req;
}

// Creates a non-proxy request, that can be resolved locally or remotely.
// Normalized args, enabling subrequests.
template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_adder_v1n_impl(A a, B b)
{
    using props_type = adder_v1_normal_props;
    return rq_function(
        props_type{request_uuid{adder_v1n_uuid}, adder_v1_title},
        adder_v1_func,
        normalize_arg<int, props_type>(std::move(a)),
        normalize_arg<int, props_type>(std::move(b)));
}

// Like previous, passing optional containment data
template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_adder_v1n_impl(containment_data const* containment, A a, B b)
{
    auto req{rq_test_adder_v1n_impl(a, b)};
    if (containment)
    {
        req.set_containment(*containment);
    }
    return req;
}

cppcoro::task<int>
coro_v1_func(context_intf& ctx, int loops, int delay);

// Creates a non-proxy request, that can be resolved locally or remotely.
// Plain args, disabling the option of subrequests.
inline auto
rq_test_coro_v1p_impl(int a, int b)
{
    using props_type = coro_v1_normal_props<caching_level_type::none>;
    return rq_function(
        props_type{request_uuid{coro_v1p_uuid}, coro_v1_title},
        coro_v1_func,
        a,
        b);
}

// Like previous, passing optional containment data
inline auto
rq_test_coro_v1p_impl(containment_data const* containment, int a, int b)
{
    auto req{rq_test_coro_v1p_impl(a, b)};
    if (containment)
    {
        req.set_containment(*containment);
    }
    return req;
}

// Creates a non-proxy request, that can be resolved locally or remotely.
// Normalized args, enabling subrequests.
template<
    caching_level_type Level = caching_level_type::none,
    typename A,
    typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_coro_v1n_impl(A a, B b)
{
    using props_type = coro_v1_normal_props<Level>;
    request_uuid uuid{coro_v1n_uuid};
    uuid.set_level(Level);
    return rq_function(
        props_type{std::move(uuid), coro_v1_title},
        coro_v1_func,
        normalize_arg<int, props_type>(std::move(a)),
        normalize_arg<int, props_type>(std::move(b)));
}

// Like previous, passing optional containment data
template<
    caching_level_type Level = caching_level_type::none,
    typename A,
    typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_coro_v1n_impl(containment_data const* containment, A a, B b)
{
    auto req{rq_test_coro_v1n_impl<Level>(a, b)};
    if (containment)
    {
        req.set_containment(*containment);
    }
    return req;
}

} // namespace cradle

#endif
