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
#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/service/request.h>
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/typing/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][service][seri_catalog]";

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
    return function_request_erased<Value, props_type>(
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
    Request const& req,
    auto& deserialize_request,
    auto& validate_request,
    std::string const& filename)
{
    // Validate original request
    validate_request(req);

    // Serialize the original request
    {
        std::ofstream ofs(filename);
        cereal::JSONOutputArchive oarchive(ofs);
        // Not
        //   oarchive(req);
        // which adds an undesired outer element.
        req.save(oarchive);
    }

    // Deserialize and verify the resulting request equals the original
    std::ifstream ifs(filename);
    cereal::JSONInputArchive iarchive(ifs);
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
    validate_request(req1);
}

// Make a "post ISS object" request, where the payload is a blob
template<caching_level_type Level>
auto
make_post_iss_request_constant()
{
    return rq_post_iss_object<Level>(
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"));
}

// Make a "post ISS object" request, where the payload comes
// from a sub-request
template<caching_level_type Level>
auto
make_post_iss_request_subreq(std::string payload = "payload")
{
    auto make_blob_coro = [](context_intf& ctx, std::string const& payload)
        -> cppcoro::task<blob> { co_return make_blob(payload); };
    std::string uuid_text{fmt::format("uuid_100_{}", static_cast<int>(Level))};
    thinknode_request_props<Level> props{
        request_uuid(uuid_text), "make_blob_coro"};
    auto make_blob_request = rq_function_erased_coro<blob>(
        props, make_blob_coro, rq_value(payload));
    return rq_post_iss_object<Level>(
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob_request);
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
    std::vector<Request> const& requests,
    std::vector<blob> const& payloads,
    std::vector<std::string> const& results,
    bool introspective = false,
    bool remotely = false)
{
    constexpr caching_level_type level = Request::caching_level;
    clean_tasklet_admin_fixture fixture;
    service_core service;
    init_test_service(service);
    if (remotely)
    {
        ensure_thinknode_seri_resolvers();
        ensure_loopback_service();
    }

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
    auto& mock_http = enable_http_mocking(service);
    mock_http.set_script(script);

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    tasklet_tracker* tasklet = nullptr;
    if (introspective)
    {
        tasklet = create_tasklet_tracker("my_pool", "my_title");
    }
    thinknode_request_context ctx{service, session, tasklet, remotely};
    ctx.proxy_name("loopback");

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res == results);
    REQUIRE(mock_http.is_complete());
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

    if constexpr (level >= caching_level_type::memory)
    {
        // Resolve using memory cache
        auto res1 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
        REQUIRE(res1 == results);
    }

    if constexpr (level >= caching_level_type::full)
    {
        sync_wait_write_disk_cache(service);
        service.inner_reset_memory_cache();

        // Resolve using disk cache
        auto res2 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
        REQUIRE(res2 == results);
    }
}

// Test a single "post ISS object" request
template<typename Request>
void
test_post_iss_request(
    Request const& req, bool introspective, bool remotely = false)
{
    std::vector<Request> requests = {req};
    auto payload = make_blob("payload");
    std::vector<blob> payloads = {payload};
    std::vector<std::string> results = {"def"};

    test_post_iss_requests_parallel(
        requests, payloads, results, introspective, remotely);
}

} // namespace

TEST_CASE("ISS POST serialization - value", tag)
{
    auto req{make_post_iss_request_constant<caching_level_type::full>()};
    // With this test_request lambda, testing serialization includes testing
    // that the deserialized request can be resolved.
    auto test_request
        = [](auto const& req1) { test_post_iss_request(req1, false); };
    test_serialize_thinknode_request(
        req,
        deserialize_function<decltype(req)>,
        test_request,
        "iss_post_value.json");
}

TEST_CASE("ISS POST serialization - subreq", tag)
{
    auto req{make_post_iss_request_subreq<caching_level_type::full>()};
    auto test_request
        = [](auto const& req1) { test_post_iss_request(req1, false); };
    test_serialize_thinknode_request(
        req,
        deserialize_function<decltype(req)>,
        test_request,
        "iss_post_subreq.json");
}

TEST_CASE("ISS POST resolution - value, uncached", tag)
{
    test_post_iss_request(
        make_post_iss_request_constant<caching_level_type::none>(), false);
}

TEST_CASE("ISS POST resolution - value, memory cached", tag)
{
    test_post_iss_request(
        make_post_iss_request_subreq<caching_level_type::memory>(), false);
}

TEST_CASE("ISS POST resolution - value, fully cached", tag)
{
    test_post_iss_request(
        make_post_iss_request_subreq<caching_level_type::full>(), false);
}

TEST_CASE("ISS POST resolution - subreq, fully cached, parallel", tag)
{
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

    test_post_iss_requests_parallel(requests, payloads, results);
}

TEST_CASE("ISS POST resolution - remote", tag)
{
    test_post_iss_request(
        make_post_iss_request_constant<caching_level_type::full>(),
        false,
        true);
}

TEST_CASE("RESOLVE ISS OBJECT TO IMMUTABLE serialization", tag)
{
    auto req{rq_resolve_iss_object_to_immutable<caching_level_type::full>(
        "123", "abc", true)};
    auto test_request = [](auto const& req1) {};
    test_serialize_thinknode_request(
        req,
        deserialize_function<decltype(req)>,
        test_request,
        "resolve_iss_object_to_immutable.json");
}

template<typename Request>
void
test_retrieve_immutable_object_parallel(
    std::vector<Request> const& requests,
    std::vector<std::string> const& object_ids,
    std::vector<std::string> const& responses)
{
    constexpr caching_level_type level = Request::caching_level;
    service_core service;
    init_test_service(service);

    mock_http_script script;
    for (unsigned i = 0; i < object_ids.size(); ++i)
    {
        mock_http_exchange exchange{
            make_get_request(
                std::string("https://mgh.thinknode.io/api/v1.0/iss/immutable/")
                    + object_ids[i] + "?context=123",
                {{"Authorization", "Bearer xyz"},
                 {"Accept", "application/octet-stream"}}),
            make_http_200_response(responses[i])};
        script.push_back(exchange);
    }
    auto& mock_http = enable_http_mocking(service);
    mock_http.set_script(script);

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    tasklet_tracker* tasklet = nullptr;
    thinknode_request_context ctx{service, session, tasklet};

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    std::vector<blob> results;
    std::ranges::transform(
        responses,
        std::back_inserter(results),
        [](std::string const& resp) -> blob { return make_blob(resp); });
    REQUIRE(res == results);
    REQUIRE(mock_http.is_complete());
    // Order unspecified so don't check mock_http.is_in_order()

    if constexpr (level >= caching_level_type::memory)
    {
        // Resolve using memory cache
        auto res1 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
        REQUIRE(res1 == results);
    }

    if constexpr (level >= caching_level_type::full)
    {
        sync_wait_write_disk_cache(service);
        service.inner_reset_memory_cache();

        // Resolve using disk cache
        auto res2 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
        REQUIRE(res2 == results);
    }
}

template<typename Request>
void
test_retrieve_immutable_object_req(Request const& req)
{
    std::vector<Request> requests = {req};
    std::vector<std::string> object_ids = {"abc"};
    std::vector<std::string> responses = {"payload"};

    test_retrieve_immutable_object_parallel(requests, object_ids, responses);
}

static void
test_retrieve_immutable_object(auto create_req, auto immutable_id)
{
    string context_id{"123"};
    auto req{create_req(context_id, immutable_id)};
    test_retrieve_immutable_object_req(req);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT resolution - value, fully cached", tag)
{
    test_retrieve_immutable_object(
        rq_retrieve_immutable_object<caching_level_type::full, std::string>,
        "abc");
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT resolution - subreq, fully cached", tag)
{
    constexpr caching_level_type level = caching_level_type::full;
    auto arg_request{rq_function_thinknode_value<level>("abc")};
    test_retrieve_immutable_object(
        rq_retrieve_immutable_object<level, decltype(arg_request)>,
        rq_function_thinknode_value<level>("abc"));
}

TEST_CASE(
    "RETRIEVE IMMUTABLE OBJECT resolution - value, fully cached, parallel",
    tag)
{
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
    test_retrieve_immutable_object_parallel(requests, object_ids, responses);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT serialization - function", tag)
{
    constexpr caching_level_type level = caching_level_type::full;
    auto req{rq_retrieve_immutable_object<level>("123", "abc")};
    test_serialize_thinknode_request(
        req,
        deserialize_function<decltype(req)>,
        test_retrieve_immutable_object_req<decltype(req)>,
        "retrieve_immutable.json");
}

TEST_CASE("Composite request serialization", tag)
{
    constexpr auto level = caching_level_type::full;
    std::string const context_id{"123"};
    auto req0{rq_post_iss_object<level>(
        context_id,
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"))};
    auto req1{
        rq_resolve_iss_object_to_immutable<level>(context_id, req0, true)};
    auto req2{rq_retrieve_immutable_object<level>(context_id, req1)};
    auto test_request = [](auto const& req1) {};
    test_serialize_thinknode_request(
        req2,
        deserialize_function<decltype(req2)>,
        test_request,
        "composite.json");
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT creation - template arg", tag)
{
    constexpr caching_level_type level = caching_level_type::full;
    std::string const context_id{"123"};
    auto coro = [](context_intf& ctx) -> cppcoro::task<std::string> {
        co_return std::string{"my_immutable_id}"};
    };
    auto req0{rq_retrieve_immutable_object<level>(
        context_id,
        rq_function_erased_coro<std::string>(
            thinknode_request_props<level>{make_uuid(), "arg"}, coro))};
    // The second argument in req1 will be "normalized" to the same thing
    // passed to req0.
    auto req1{
        rq_retrieve_immutable_object<level>(context_id, "my_immutable_id")};

    REQUIRE(typeid(req0) == typeid(req1));
    REQUIRE(req0.get_uuid().str() == req1.get_uuid().str());

#if 0
// TODO support arguments from any request kind
    auto req2{rq_retrieve_immutable_object<level>(context_id,
        rq_value(std::string{"my_immutable_id}"}))};
    REQUIRE(typeid(req0) == typeid(req2));
    REQUIRE(req0.get_uuid().str() == req2.get_uuid().str());
#endif
}
