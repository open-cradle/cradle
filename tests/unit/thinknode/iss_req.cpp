#include <algorithm>
#include <fstream>
#include <iterator>
#include <type_traits>
#include <typeinfo>

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "../../support/concurrency_testing.h"
#include "../../support/inner_service.h"
#include "../../support/request.h"
#include "../../support/tasklet_testing.h"
#include "../../support/thinknode.h"
#include "../../thinknode-dll/t0/make_some_blob_t0.h"
#include "../../thinknode-dll/t0/make_some_blob_t0_impl.h"
#include "../../thinknode-dll/t0/seri_catalog_t0.h"
#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>
#include <cradle/test_dlls_dir.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

namespace {

static char const tag[] = "[thinknode][iss_req]";

template<typename Request>
auto
deserialize_function(JSONRequestInputArchive& iarchive)
{
    using value_type = typename Request::value_type;
    using props_type = typename Request::props_type;
    return function_request<value_type, props_type>(iarchive);
}

template<typename Value>
Value
identity(Value&& value)
{
    return std::forward<Value>(value);
}

request_uuid
make_uuid()
{
    static int next_id{};
    return request_uuid{fmt::format("{}-{}", tag, next_id++)};
}

// Creates a type-erased, uncached, request for an immediate value in Thinknode
// context.
template<caching_level_type Level, typename Value>
auto
rq_function_thinknode_value(Value&& value)
{
    using props_type = thinknode_request_props<Level>;
    return function_request<Value, props_type>(
        props_type{make_uuid(), "rq_function_thinknode_value"},
        identity_coro<Value>,
        std::forward<Value>(value));
}

template<caching_level_type Level>
auto
rq_function_thinknode_value(char const* const value)
{
    return rq_function_thinknode_value<Level, std::string>(value);
}

template<typename Request>
void
test_serialize_thinknode_request(
    thinknode_test_scope& scope,
    Request const& req,
    auto& deserialize_request,
    auto& validate_request,
    std::string const& filename)
{
    // Validate original request
    validate_request(scope, req);

    // Serialize the original request
    {
        std::ofstream ofs(filename);
        JSONRequestOutputArchive oarchive(ofs);
        // Not
        //   oarchive(req);
        // which adds an undesired outer element.
        req.save(oarchive);
    }

    // Deserialize and verify the resulting request equals the original
    std::ifstream ifs(filename);
    JSONRequestInputArchive iarchive{ifs, scope.get_resources()};
    Request req1{deserialize_request(iarchive)};
    REQUIRE(req1.hash() == req.hash());
    REQUIRE(
        get_unique_string(*req1.get_captured_id())
        == get_unique_string(*req.get_captured_id()));
    if constexpr (Request::introspective)
    {
        REQUIRE(
            req1.get_introspection_title() == req.get_introspection_title());
    }
    validate_request(scope, req1);
}

// Make a "post ISS object" request, where the payload is a blob
template<caching_level_type Level = caching_level_type::full>
auto
make_post_iss_request_constant()
{
    return rq_post_iss_object<Level>(
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"));
}

// Make a "post ISS object" proxy request, where the payload is a blob
auto
make_post_iss_proxy_request_constant()
{
    return rq_proxy_post_iss_object(
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"));
}

// Make a "post ISS object" request, where the payload comes
// from a subrequest.
// Deserializing or resolving this request requires DLL test_thinknode_dll_t0
// on the machine (local/remote) that is performing the operation.
template<caching_level_type Level>
auto
make_post_iss_request_subreq(std::string payload = "payload")
{
    return rq_post_iss_object<Level>(
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        rq_make_test_blob<Level>(std::move(payload)));
}

// Make a "post ISS object" proxy request, where the payload comes
// from a subrequest.
// Deserializing or resolving this request requires DLL test_thinknode_dll_t0
// on the remote.
auto
make_post_iss_proxy_request_subreq(std::string payload = "payload")
{
    return rq_proxy_post_iss_object(
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        rq_proxy_make_test_blob(std::move(payload)));
}

// Test resolving a number of "post ISS object" requests in parallel
// - results[i] is the result for requests[i]
// - results[i] is the result for payloads[i] for i < payloads.size()
// The values in payloads are unique; so
// - requests.size() == results.size()
// - payloads.size() <= results.size()
template<typename Request>
void
test_post_iss_requests_parallel(
    thinknode_test_scope& scope,
    std::vector<Request> const& requests,
    std::vector<blob> const& payloads,
    std::vector<std::string> const& results,
    bool introspective)
{
    constexpr caching_level_type level = Request::caching_level;
    scope.clear_caches();
    auto& resources{scope.get_resources()};
    clean_tasklet_admin_fixture fixture;

    mock_http_session* mock_http{nullptr};
    if (auto proxy = scope.get_proxy())
    {
        // Assumes a single request/response
        auto response{
            make_http_200_response("{ \"id\": \"" + results[0] + "\" }")};
        // TODO should body be blob or string?
        auto const& body{response.body};
        std::string s{reinterpret_cast<char const*>(body.data()), body.size()};
        proxy->mock_http(s);
    }
    else
    {
        mock_http_script script;
        for (unsigned i = 0; i < payloads.size(); ++i)
        {
            mock_http_exchange exchange{
                make_http_request(
                    http_request_method::POST,
                    "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
                    {{"Authorization", "Bearer xyz"},
                     {"Accept", "application/json"},
                     {"Content-Type", "application/octet-stream"}},
                    payloads[i]),
                make_http_200_response("{ \"id\": \"" + results[i] + "\" }")};
            script.push_back(exchange);
        }
        mock_http = &resources.enable_http_mocking();
        mock_http->set_script(script);
    }

    tasklet_tracker* tasklet = nullptr;
    if (introspective)
    {
        tasklet = create_tasklet_tracker("my_pool", "my_title");
    }
    auto ctx{scope.make_context(tasklet)};

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res == results);
    if (mock_http)
    {
        CHECK(mock_http->is_complete());
    }
    // Order unspecified so don't check mock_http.is_in_order()
    if (introspective)
    {
        auto infos = get_tasklet_infos(true);
        // my_post_iss_object_request, HTTP request
        REQUIRE(infos.size() == 2);
        REQUIRE(infos[0].pool_name() == "my_pool");
        REQUIRE(infos[0].title() == "my_title");
        // scheduled, before_co_await, ...
        REQUIRE(infos[0].events().size() >= 2);
        auto const& event01 = infos[0].events()[1];
        REQUIRE(event01.what() == tasklet_event_type::BEFORE_CO_AWAIT);
        REQUIRE(event01.details().starts_with("my_post_iss_object_request"));
        REQUIRE(infos[1].pool_name() == "HTTP");
        REQUIRE(infos[1].title() == "HTTP: post https://mgh.thinknode.io/api/v1.0/iss/string?context=123");
    }

    if constexpr (is_cached(level))
    {
        // Resolve using memory cache
        auto res1 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
        REQUIRE(res1 == results);
    }

    if constexpr (is_fully_cached(level))
    {
        sync_wait_write_disk_cache(resources);
        resources.reset_memory_cache();

        // Resolve using disk cache
        auto res2 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
        REQUIRE(res2 == results);
    }
}

// Test a single "post ISS object" request
template<typename Request>
void
test_post_iss_request(thinknode_test_scope& scope, Request const& req)
{
    std::vector<Request> requests = {req};
    auto payload = make_blob("payload");
    std::vector<blob> payloads = {payload};
    std::vector<std::string> results = {"def"};
    bool introspective = false;

    test_post_iss_requests_parallel(
        scope, requests, payloads, results, introspective);
}

} // namespace

TEST_CASE("ISS POST serialization - value", tag)
{
    thinknode_test_scope scope;
    auto req{make_post_iss_request_constant()};
    //  With this validate_request lambda, testing serialization includes
    //  verifying that the deserialized request can be locally resolved.
    test_serialize_thinknode_request(
        scope,
        req,
        deserialize_function<decltype(req)>,
        test_post_iss_request<decltype(req)>,
        "iss_post_value.json");
}

TEST_CASE("ISS POST serialization - subreq", tag)
{
    thinknode_test_scope scope;
    seri_catalog_t0 cat{scope.get_resources().get_seri_registry()};
    auto req{make_post_iss_request_subreq<caching_level_type::full>()};
    test_serialize_thinknode_request(
        scope,
        req,
        deserialize_function<decltype(req)>,
        test_post_iss_request<decltype(req)>,
        "iss_post_subreq.json");
}

TEST_CASE("ISS POST resolution - value, uncached", tag)
{
    thinknode_test_scope scope;
    test_post_iss_request(
        scope, make_post_iss_request_constant<caching_level_type::none>());
}

TEST_CASE("ISS POST resolution - subreq, memory cached", tag)
{
    thinknode_test_scope scope;
    test_post_iss_request(
        scope, make_post_iss_request_subreq<caching_level_type::memory>());
}

TEST_CASE("ISS POST resolution - value, fully cached", tag)
{
    thinknode_test_scope scope;
    test_post_iss_request(
        scope, make_post_iss_request_subreq<caching_level_type::full>());
}

TEST_CASE("ISS POST resolution - subreq, fully cached, parallel", tag)
{
    thinknode_test_scope scope;
    constexpr caching_level_type level = caching_level_type::full;
    using Req = decltype(make_post_iss_request_subreq<level>());
    std::vector<blob> payloads;
    std::vector<Req> requests;
    std::vector<std::string> results;
    for (int i = 0; i < 7; ++i)
    {
        // 7 requests / results, but only
        // 3 unique payloads / requests / results
        int req_id = i % 3;
        std::ostringstream ss_payload;
        ss_payload << "payload_" << req_id;
        auto payload = ss_payload.str();
        if (req_id == i)
        {
            payloads.emplace_back(make_blob(payload));
        }

        requests.emplace_back(make_post_iss_request_subreq<level>(payload));

        std::ostringstream ss_result;
        ss_result << "result_" << req_id;
        auto result = ss_result.str();
        results.emplace_back(result);
    }

    test_post_iss_requests_parallel(scope, requests, payloads, results, false);
}

TEST_CASE("ISS POST resolution - value, loopback", tag)
{
    thinknode_test_scope scope{"loopback"};
    test_post_iss_request(scope, make_post_iss_request_constant());
}

TEST_CASE("ISS POST resolution - subreq, loopback", tag)
{
    thinknode_test_scope scope{"loopback"};
    scope.get_proxy()->load_shared_library(
        get_test_dlls_dir(), "test_thinknode_dll_t0");
    test_post_iss_request(
        scope, make_post_iss_request_subreq<caching_level_type::full>());
}

TEST_CASE("ISS POST resolution - value, proxy, loopback", tag)
{
    thinknode_test_scope scope{"loopback"};
    test_post_iss_request(scope, make_post_iss_proxy_request_constant());
}

TEST_CASE("ISS POST resolution - subreq, proxy, loopback", tag)
{
    thinknode_test_scope scope{"loopback"};
    scope.get_proxy()->load_shared_library(
        get_test_dlls_dir(), "test_thinknode_dll_t0");
    test_post_iss_request(scope, make_post_iss_proxy_request_subreq());
}

TEST_CASE("ISS POST resolution - value, rpclib", tag)
{
    thinknode_test_scope scope{"rpclib"};
    test_post_iss_request(scope, make_post_iss_request_constant());
}

TEST_CASE("ISS POST resolution - subreq, rpclib", tag)
{
    thinknode_test_scope scope{"rpclib"};
    scope.get_proxy()->load_shared_library(
        get_test_dlls_dir(), "test_thinknode_dll_t0");
    test_post_iss_request(
        scope, make_post_iss_request_subreq<caching_level_type::full>());
}

TEST_CASE("ISS POST resolution - value, proxy, rpclib", tag)
{
    thinknode_test_scope scope{"rpclib"};
    test_post_iss_request(scope, make_post_iss_proxy_request_constant());
}

TEST_CASE("ISS POST resolution - subreq, proxy, rpclib", tag)
{
    thinknode_test_scope scope{"rpclib"};
    scope.get_proxy()->load_shared_library(
        get_test_dlls_dir(), "test_thinknode_dll_t0");
    test_post_iss_request(scope, make_post_iss_proxy_request_subreq());
}

TEST_CASE("RESOLVE ISS OBJECT TO IMMUTABLE serialization", tag)
{
    thinknode_test_scope scope;
    constexpr caching_level_type level = caching_level_type::full;
    auto req{rq_resolve_iss_object_to_immutable<level>("123", "abc", true)};
    auto validate_request = [](thinknode_test_scope&, auto const& req1) {};
    test_serialize_thinknode_request(
        scope,
        req,
        deserialize_function<decltype(req)>,
        validate_request,
        "resolve_iss_object_to_immutable.json");
}

namespace {

// Not introspective
template<typename Request>
void
test_retrieve_immutable_object_parallel(
    thinknode_test_scope& scope,
    std::vector<Request> const& requests,
    std::vector<std::string> const& object_ids,
    std::vector<std::string> const& responses)
{
    constexpr caching_level_type level = Request::caching_level;
    scope.clear_caches();
    auto& resources{scope.get_resources()};
    mock_http_session* mock_http{nullptr};
    if (auto proxy = scope.get_proxy())
    {
        // Assumes a single request/response
        auto response{make_http_200_response(responses[0])};
        // TODO should body be blob or string?
        auto const& body{response.body};
        std::string s{reinterpret_cast<char const*>(body.data()), body.size()};
        proxy->mock_http(s);
    }
    else
    {
        mock_http_script script;
        for (unsigned i = 0; i < object_ids.size(); ++i)
        {
            mock_http_exchange exchange{
                make_get_request(
                    std::string(
                        "https://mgh.thinknode.io/api/v1.0/iss/immutable/")
                        + object_ids[i] + "?context=123",
                    {{"Authorization", "Bearer xyz"},
                     {"Accept", "application/octet-stream"}}),
                make_http_200_response(responses[i])};
            script.push_back(exchange);
        }
        mock_http = &resources.enable_http_mocking();
        mock_http->set_script(script);
    }

    auto ctx{scope.make_context()};

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    std::vector<blob> results;
    std::ranges::transform(
        responses,
        std::back_inserter(results),
        [](std::string const& resp) -> blob { return make_blob(resp); });
    REQUIRE(res == results);
    if (mock_http)
    {
        CHECK(mock_http->is_complete());
    }
    // Order unspecified so don't check mock_http.is_in_order()

    // TODO only local?
    if constexpr (is_cached(level))
    {
        // Resolve using memory cache
        auto res1 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
        REQUIRE(res1 == results);
    }

    if constexpr (is_fully_cached(level))
    {
        sync_wait_write_disk_cache(resources);
        resources.reset_memory_cache();

        // Resolve using disk cache
        auto res2 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
        REQUIRE(res2 == results);
    }
}

template<typename Request>
void
test_retrieve_immutable_object(thinknode_test_scope& scope, Request const& req)
{
    std::vector<Request> requests = {req};
    std::vector<std::string> object_ids = {"abc"};
    std::vector<std::string> responses = {"payload"};

    test_retrieve_immutable_object_parallel(
        scope, requests, object_ids, responses);
}

} // namespace

TEST_CASE("RETRIEVE IMMUTABLE OBJECT creation - template arg", tag)
{
    thinknode_test_scope scope;
    constexpr caching_level_type level = caching_level_type::full;
    std::string const context_id{"123"};
    std::string const object_id{"abc"}; // same as above
    auto ctx{scope.make_context()};

    auto coro = [=](context_intf& ctx) -> cppcoro::task<std::string> {
        co_return object_id;
    };
    auto req0{rq_retrieve_immutable_object<level>(
        context_id,
        rq_function(
            thinknode_request_props<level>{make_uuid(), "arg"}, coro))};
    test_retrieve_immutable_object(scope, req0);

    // The second argument in req1 is "normalized" to the same thing
    // passed to req0.
    auto req1{rq_retrieve_immutable_object<level>(context_id, object_id)};
    REQUIRE(typeid(req0) == typeid(req1));
    test_retrieve_immutable_object(scope, req1);

    // A value request is normalized in the same way.
    auto req2{
        rq_retrieve_immutable_object<level>(context_id, rq_value(object_id))};
    REQUIRE(typeid(req0) == typeid(req2));
    test_retrieve_immutable_object(scope, req2);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT serialization", tag)
{
    thinknode_test_scope scope;
    constexpr caching_level_type level = caching_level_type::full;
    auto req{rq_retrieve_immutable_object<level>("123", "abc")};
    test_serialize_thinknode_request(
        scope,
        req,
        deserialize_function<decltype(req)>,
        test_retrieve_immutable_object<decltype(req)>,
        "retrieve_immutable.json");
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT resolution - value, local", tag)
{
    thinknode_test_scope scope;
    constexpr caching_level_type level = caching_level_type::full;
    test_retrieve_immutable_object(
        scope, rq_retrieve_immutable_object<level>("123", "abc"));
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT resolution - subreq, local", tag)
{
    thinknode_test_scope scope;
    constexpr caching_level_type level = caching_level_type::full;
    test_retrieve_immutable_object(
        scope,
        rq_retrieve_immutable_object<level>(
            "123", rq_function_thinknode_value<level>("abc")));
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT resolution - value, local, parallel", tag)
{
    thinknode_test_scope scope;
    constexpr caching_level_type level = caching_level_type::full;
    using Req = decltype(rq_retrieve_immutable_object<level>("", ""));
    std::vector<std::string> object_ids;
    std::vector<Req> requests;
    std::vector<std::string> responses;
    for (int i = 0; i < 7; ++i)
    {
        int req_id = i % 3;
        std::ostringstream ss_object_id;
        ss_object_id << "abc" << req_id;
        auto object_id = ss_object_id.str();
        if (req_id == i)
        {
            object_ids.push_back(object_id);
        }

        requests.push_back(
            rq_retrieve_immutable_object<level>("123", object_id));

        std::ostringstream ss_response;
        ss_response << "payload_" << req_id;
        auto response = ss_response.str();
        responses.push_back(response);
    }
    test_retrieve_immutable_object_parallel(
        scope, requests, object_ids, responses);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - loopback", tag)
{
    thinknode_test_scope scope{"loopback"};
    constexpr caching_level_type level = caching_level_type::full;
    test_retrieve_immutable_object(
        scope, rq_retrieve_immutable_object<level>("123", "abc"));
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - proxy, loopback", tag)
{
    thinknode_test_scope scope{"loopback"};
    test_retrieve_immutable_object(
        scope, rq_proxy_retrieve_immutable_object("123", "abc"));
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - rpclib", tag)
{
    thinknode_test_scope scope{"rpclib"};
    constexpr caching_level_type level = caching_level_type::full;
    test_retrieve_immutable_object(
        scope, rq_retrieve_immutable_object<level>("123", "abc"));
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - proxy, rpclib", tag)
{
    thinknode_test_scope scope{"rpclib"};
    test_retrieve_immutable_object(
        scope, rq_proxy_retrieve_immutable_object("123", "abc"));
}

TEST_CASE("Composite request serialization", tag)
{
    thinknode_test_scope scope;
    constexpr auto level = caching_level_type::full;
    std::string const context_id{"123"};
    auto req0{rq_post_iss_object<level>(
        context_id,
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"))};
    auto req1{
        rq_resolve_iss_object_to_immutable<level>(context_id, req0, true)};
    auto req2{rq_retrieve_immutable_object<level>(context_id, req1)};
    auto validate_request = [](thinknode_test_scope&, auto const& req1) {};
    test_serialize_thinknode_request(
        scope,
        req2,
        deserialize_function<decltype(req2)>,
        validate_request,
        "composite.json");
}
