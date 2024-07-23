#ifndef CRADLE_TESTS_INNER_DLL_M0_META_M0_META_H
#define CRADLE_TESTS_INNER_DLL_M0_META_M0_META_H

#include <string>
#include <vector>

#include <cereal/types/vector.hpp>

#include "m0_meta_defs.h"
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

class containment_data;

// Creates a proxy request that will be resolved remotely.
// Plain args, disabling the option of subrequests.
inline auto
rq_test_m0_metap(int a, int b)
{
    using props_type = m0_proxy_props_type;
    request_uuid uuid{m0_meta_p_uuid};
    uuid.set_level(caching_level_type::none);
    return rq_proxy<m0_inner_request_type>(
        props_type{std::move(uuid), m0_meta_title}, a, b);
}

// Like previous, passing optional containment data
inline auto
rq_test_m0_metap(containment_data const* containment, int a, int b)
{
    auto req{rq_test_m0_metap(a, b)};
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
rq_test_m0_metan(A a, B b)
{
    using props_type = m0_proxy_props_type;
    request_uuid uuid{m0_meta_n_uuid};
    uuid.set_level(caching_level_type::none);
    return rq_proxy<m0_inner_request_type>(
        props_type{std::move(uuid), m0_meta_title},
        normalize_arg<int, props_type>(std::move(a)),
        normalize_arg<int, props_type>(std::move(b)));
}

// Like previous, passing optional containment data
template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
auto
rq_test_m0_metan(containment_data const* containment, A a, B b)
{
    auto req{rq_test_m0_metan(a, b)};
    if (containment)
    {
        req.set_containment(*containment);
    }
    return req;
}

// Creates a metavec proxy request that will be resolved remotely.
// Plain args, disabling the option of subrequests.
inline auto
rq_test_m0_metavecp(std::vector<int> input)
{
    using props_type = m0_proxy_props_type;
    request_uuid uuid{m0_metavec_p_uuid};
    uuid.set_level(caching_level_type::none);
    return rq_proxy<std::vector<m0_inner_request_type>>(
        props_type{std::move(uuid), m0_metavec_title}, std::move(input));
}

} // namespace cradle

#endif
