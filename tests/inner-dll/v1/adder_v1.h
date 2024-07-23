#ifndef CRADLE_TESTS_INNER_DLL_V1_ADDER_V1_H
#define CRADLE_TESTS_INNER_DLL_V1_ADDER_V1_H

#include <string>

#include "adder_v1_defs.h"
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

class containment_data;

// Creates a proxy request that will be resolved remotely.
// Plain args, disabling the option of subrequests.
inline auto
rq_test_adder_v1p(int a, int b)
{
    using props_type = adder_v1_proxy_props;
    return rq_proxy<int>(
        props_type{request_uuid{adder_v1p_uuid}, adder_v1_title}, a, b);
}

// Like previous, passing optional containment data
inline auto
rq_test_adder_v1p(containment_data const* containment, int a, int b)
{
    auto req{rq_test_adder_v1p(a, b)};
    if (containment)
    {
        req.set_containment(*containment);
    }
    return req;
}

// Creates a proxy request that will be resolved remotely.
// Normalized args, enabling subrequests.
template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_adder_v1n(A a, B b)
{
    using props_type = adder_v1_proxy_props;
    return rq_proxy<int>(
        props_type{request_uuid{adder_v1n_uuid}, adder_v1_title},
        normalize_arg<int, props_type>(std::move(a)),
        normalize_arg<int, props_type>(std::move(b)));
}

// Like previous, passing optional containment data
template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_adder_v1n(containment_data const* containment, A a, B b)
{
    auto req{rq_test_adder_v1n(a, b)};
    if (containment)
    {
        req.set_containment(*containment);
    }
    return req;
}

// Creates a proxy request that will be resolved remotely.
// Plain args, disabling the option of subrequests.
inline auto
rq_test_coro_v1p(int a, int b)
{
    using props_type = coro_v1_proxy_props;
    return rq_proxy<int>(
        props_type{request_uuid{coro_v1p_uuid}, coro_v1_title}, a, b);
}

// Like previous, passing optional containment data
inline auto
rq_test_coro_v1p(containment_data const* containment, int a, int b)
{
    auto req{rq_test_coro_v1p(a, b)};
    if (containment)
    {
        req.set_containment(*containment);
    }
    return req;
}

// Creates a proxy request that will be resolved remotely.
// Normalized args, enabling subrequests.
template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_coro_v1n(A a, B b)
{
    using props_type = coro_v1_proxy_props;
    request_uuid uuid{coro_v1n_uuid};
    uuid.set_level(caching_level_type::none);
    return rq_proxy<int>(
        props_type{std::move(uuid), coro_v1_title},
        normalize_arg<int, props_type>(std::move(a)),
        normalize_arg<int, props_type>(std::move(b)));
}

// Like previous, passing optional containment data
template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_coro_v1n(containment_data const* containment, A a, B b)
{
    auto req{rq_test_coro_v1n(a, b)};
    if (containment)
    {
        req.set_containment(*containment);
    }
    return req;
}

} // namespace cradle

#endif
