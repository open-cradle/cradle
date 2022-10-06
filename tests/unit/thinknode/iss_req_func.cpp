#include <algorithm>
#include <fstream>
#include <iterator>
#include <type_traits>

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cppcoro/sync_wait.hpp>
#include <spdlog/spdlog.h>

#include "../../inner/introspection/tasklet_testing.h"
#include "../../inner/support/concurrency_testing.h"
#include "../../inner/support/request.h"
#include "iss_req_common.h"
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/service/request.h>
#include <cradle/thinknode/iss_req_func.h>
#include <cradle/typing/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

template<typename Request>
auto
deserialize_function(cereal::JSONInputArchive& iarchive)
{
    using value_type = typename Request::value_type;
    using props_type = typename Request::props_type;
    return function_request_erased<value_type, props_type>(iarchive);
}

template<typename Value>
Value
identity(Value&& value)
{
    return std::forward<Value>(value);
}

// Creates a type-erased, uncached, request for an immediate value in Thinknode
// context.
// The request has no real uuid, meaning it cannot be serialized.
// As it won't be cached, the absence of a uuid is no obstacle there.
template<typename Value>
auto
rq_function_thinknode_value(Value&& value)
{
    using props_type = request_props<caching_level_type::none, false, false>;
    return function_request_erased<Value, props_type>(
        props_type(), identity<Value>, std::forward<Value>(value));
}

static auto
rq_function_thinknode_value(char const* const value)
{
    return rq_function_thinknode_value<std::string>(value);
}

TEST_CASE("ISS POST serialization - function, blob", "[iss_req_func]")
{
    auto req{rq_post_iss_object_func<caching_level_type::full>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"))};
    auto test_request
        = [](auto const& req1) { test_post_iss_request(req1, false); };
    test_serialize_thinknode_request(
        req,
        deserialize_function<decltype(req)>,
        test_request,
        "iss_post_func_blob.json");
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - plain, fully cached", "[iss_req_func]")
{
    test_retrieve_immutable_object(
        rq_retrieve_immutable_object_func<
            caching_level_type::full,
            std::string>,
        "abc");
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - subreq, fully cached", "[iss_req_func]")
{
    auto arg_request{rq_function_thinknode_value("abc")};
    test_retrieve_immutable_object(
        rq_retrieve_immutable_object_func<
            caching_level_type::full,
            decltype(arg_request)>,
        rq_function_thinknode_value("abc"));
}

TEST_CASE(
    "RETRIEVE IMMUTABLE OBJECT serialization - function", "[iss_req_func]")
{
    constexpr caching_level_type level = caching_level_type::full;
    auto req{rq_retrieve_immutable_object_func<level>(
        "https://mgh.thinknode.io/api/v1.0", "123", "abc")};
    test_serialize_thinknode_request(
        req,
        deserialize_function<decltype(req)>,
        test_retrieve_immutable_object_req<decltype(req)>,
        "retrieve_immutable_func.json");
}

TEST_CASE(
    "RESOLVE ISS OBJECT TO IMMUTABLE serialization - function",
    "[iss_req_func]")
{
    auto req{rq_resolve_iss_object_to_immutable_func<caching_level_type::full>(
        "https://mgh.thinknode.io/api/v1.0", "123", "abc", true)};
    auto test_request = [](auto const& req1) {};
    test_serialize_thinknode_request(
        req,
        deserialize_function<decltype(req)>,
        test_request,
        "resolve_iss_object_to_immutable_func.json");
}

TEST_CASE("Composite request serialization", "[iss_req_func]")
{
    constexpr auto level = caching_level_type::full;
    std::string const api_url{"https://mgh.thinknode.io/api/v1.0"};
    std::string const context_id{"123"};
    auto req0{rq_post_iss_object_func<level>(
        api_url,
        context_id,
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"))};
    auto req1{rq_resolve_iss_object_to_immutable_func<level>(
        api_url, context_id, req0, true)};
    auto req2{
        rq_retrieve_immutable_object_func<level>(api_url, context_id, req1)};
    auto test_request = [](auto const& req1) {};
    test_serialize_thinknode_request(
        req2,
        deserialize_function<decltype(req2)>,
        test_request,
        "composite.json");
}
