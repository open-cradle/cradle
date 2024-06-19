#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <msgpack.hpp>

#include "../../support/inner_service.h"
#include <cradle/inner/encodings/msgpack_packer.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/resolve/resolve_request.h>

// Code is generated for resolving a serialized request returning a request.
// We have no support for deserializing a serialized request using msgpack.
namespace msgpack {
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
{
    namespace adaptor {

    template<typename Value, typename ObjectProps>
    struct pack<cradle::function_request<Value, ObjectProps>>
    {
        template<typename Stream>
        msgpack::packer<Stream>&
        operator()(msgpack::packer<Stream>& o, cradle::function_request<Value, ObjectProps> const& v) const
        {
            static_assert(std::same_as<Stream, cradle::msgpack_ostream>);
            throw cradle::not_implemented_error{"pack function_request"};
        }
    };

    } // namespace adaptor
} // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack

using namespace cradle;

namespace {

static char const tag[] = "[inner][resolve][meta]";

using my_value_type = int;
using my_request_props_type
    = request_props<caching_level_type::none, request_function_t::coro, false>;
using my_object_props_type
    = make_request_object_props_type<my_request_props_type>;
using my_request_type = function_request<my_value_type, my_object_props_type>;

request_uuid
make_test_uuid(int ext)
{
    return request_uuid{fmt::format("{}-{:04d}", tag, ext)};
}

// Example worker function
cppcoro::task<int>
inner_function(context_intf& ctx, int x, int y)
{
    co_return x + y;
}

// Example request factory function
cppcoro::task<my_request_type>
make_inner_request(context_intf& ctx, int x, int y)
{
    my_request_props_type inner_props{make_test_uuid(100)};
    co_return rq_function(inner_props, inner_function, x, y);
}

// Generic worker function for a meta request
// InnerRequest is my_request_type
template<typename InnerRequest>
cppcoro::task<typename InnerRequest::value_type>
meta_function(context_intf& ctx, InnerRequest const& inner_req)
{
    // TODO can ctx always be used for both resolutions?
#if 1
    auto& inner_ctx{ctx};
#else
    // TODO would need to pass in context factory
    auto& resources{ctx.get_resources()};
    non_caching_request_resolution_context inner_ctx{resources};
#endif
    return resolve_request(inner_ctx, inner_req);
}

// Generic meta request factory
// MakeRequest is function_request<my_request_type, my_object_props_type>
// Returning my_request_type
// InnerRequest is my_request_type
template<typename Props, typename MakerRequest>
auto
rq_meta(Props const& meta_props, MakerRequest const& maker_req)
{
    using InnerRequest = MakerRequest::value_type;
    return rq_function(meta_props, meta_function<InnerRequest>, maker_req);
}

} // namespace

TEST_CASE("resolve meta", tag)
{
    auto resources{make_inner_test_resources()};

    my_request_props_type maker_props{make_test_uuid(101)};
    auto maker_req{rq_function(maker_props, make_inner_request, 3, 2)};

    my_request_props_type meta_props{make_test_uuid(102)};
    auto meta_req{rq_meta(meta_props, maker_req)};

    non_caching_request_resolution_context ctx{*resources};
    auto res = cppcoro::sync_wait(resolve_request(ctx, meta_req));

    REQUIRE(res == 3 + 2);
}
