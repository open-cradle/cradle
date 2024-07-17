#ifndef CRADLE_TESTS_INNER_DLL_M0_META_M0_META_IMPL_H
#define CRADLE_TESTS_INNER_DLL_M0_META_M0_META_IMPL_H

#include <vector>

#include <cereal/types/vector.hpp>
#include <cppcoro/task.hpp>

#include "m0_meta_defs.h"
#include <cradle/inner/requests/generic.h>

namespace cradle {

class context_intf;

// Creates an inner request; non-coro, intended for register_resolver()
m0_inner_request_type
m0_make_inner_request_func(int a, int b);

// Creates an inner request; coro, intended for client code
cppcoro::task<m0_inner_request_type>
m0_make_inner_request_coro(context_intf& ctx, int a, int b);

// Creates a vector of inner requests; coro, intended for client code
cppcoro::task<std::vector<m0_inner_request_type>>
m0_make_vec_inner_request_coro(context_intf& ctx, std::vector<int> input);

// Creates a meta request
template<caching_level_type Level = caching_level_type::none>
m0_meta_request_type<Level>
rq_test_m0_metap_impl(int a, int b)
{
    using props_type = m0_normal_props_type<Level>;
    request_uuid uuid{m0_meta_p_uuid};
    uuid.set_level(Level);
    return rq_function(
        props_type{std::move(uuid), m0_meta_title},
        m0_make_inner_request_coro,
        a,
        b);
}

// Creates a meta request; normalized args
template<
    caching_level_type Level = caching_level_type::none,
    typename A,
    typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_m0_metan_impl(A a, B b)
{
    using props_type = m0_normal_props_type<Level>;
    request_uuid uuid{m0_meta_n_uuid};
    uuid.set_level(Level);
    return rq_function(
        props_type{std::move(uuid), m0_meta_title},
        m0_make_inner_request_coro,
        normalize_arg<int, props_type>(std::move(a)),
        normalize_arg<int, props_type>(std::move(b)));
}

// Creates a metavec request
template<caching_level_type Level = caching_level_type::none>
m0_metavec_request_type<Level>
rq_test_m0_metavecp_impl(std::vector<int> input)
{
    using props_type = m0_normal_props_type<Level>;
    request_uuid uuid{m0_metavec_p_uuid};
    uuid.set_level(Level);
    return rq_function(
        props_type{std::move(uuid), m0_metavec_title},
        m0_make_vec_inner_request_coro,
        std::move(input));
}

} // namespace cradle

#endif
