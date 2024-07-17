#include "m0_meta_impl.h"
#include "m0_meta_defs.h"

namespace cradle {

namespace {

// Example worker function
cppcoro::task<m0_inner_value_type>
m0_meta_function(context_intf& ctx, int a, int b)
{
    co_return a + b;
}

} // namespace

m0_inner_request_type
m0_make_inner_request_func(int a, int b)
{
    using props_type = m0_normal_props_type<caching_level_type::none>;
    return rq_function(
        props_type{request_uuid{m0_inner_uuid}, m0_inner_title},
        m0_meta_function,
        a,
        b);
}

cppcoro::task<m0_inner_request_type>
m0_make_inner_request_coro(context_intf& ctx, int a, int b)
{
    // TODO what if m0_make_inner_request_func() needs ctx?
    co_return m0_make_inner_request_func(a, b);
}

cppcoro::task<std::vector<m0_inner_request_type>>
m0_make_vec_inner_request_coro(context_intf& ctx, std::vector<int> input)
{
    auto input_size = input.size();
    std::vector<m0_inner_request_type> res;
    for (decltype(input_size) i = 0; i + 1 < input_size; i += 2)
    {
        res.push_back(
            co_await m0_make_inner_request_coro(ctx, input[i], input[i + 1]));
    }
    co_return res;
}

} // namespace cradle
