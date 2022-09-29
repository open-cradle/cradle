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
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/service/request.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/typing/encodings/msgpack.h>
#include <cradle/typing/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

template<typename Request>
static void
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
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"));
}

// Make a "post ISS object" request, where the payload is a dynamic
template<caching_level_type Level>
auto
make_post_iss_request_dynamic()
{
    return rq_post_iss_object<Level>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        dynamic("payload"));
}

// Make a "post ISS object" request, where the payload comes from a
// type-erased request
template<caching_level_type Level>
auto
make_post_iss_request_request(std::string payload = "payload")
{
    std::ostringstream uuid;
    uuid << "uuid_" << payload << "_" << static_cast<int>(Level);
    auto make_blob_function
        = [](std::string const& payload) { return make_blob(payload); };
    request_props<Level> props{request_uuid(uuid.str())};
    auto make_blob_request
        = rq_function_erased(props, make_blob_function, rq_value(payload));
    return rq_post_iss_object<Level>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob_request);
}

// Make a type-erased "post ISS object" request, where the payload is a blob
template<caching_level_type Level>
auto
make_post_iss_request_erased_constant()
{
    return rq_post_iss_object_erased<Level>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"));
}

// Make a type-erased "post ISS object" request, where the payload comes
// from another type-erased request
template<caching_level_type Level>
auto
make_post_iss_request_erased_request()
{
    auto make_blob_function
        = [](std::string const& payload) { return make_blob(payload); };
    request_props<Level> props{request_uuid("uuid_100")};
    auto make_blob_request = rq_function_erased(
        props, make_blob_function, rq_value(std::string("payload")));
    return rq_post_iss_object_erased<Level>(
        "https://mgh.thinknode.io/api/v1.0",
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
static void
test_post_iss_requests_parallel(
    std::vector<Request> const& requests,
    std::vector<blob> const& payloads,
    std::vector<std::string> const& results,
    bool introspected = false)
{
    constexpr caching_level_type level = Request::caching_level;
    clean_tasklet_admin_fixture fixture;
    service_core service;
    init_test_service(service);

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
    if (introspected)
    {
        tasklet = create_tasklet_tracker("my_pool", "my_title");
    }
    thinknode_request_context ctx{service, session, tasklet};

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res == results);
    REQUIRE(mock_http.is_complete());
    // Order unspecified so don't check mock_http.is_in_order()
    if (introspected)
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
        service.inner_reset_memory_cache(
            immutable_cache_config{0x40'00'00'00});

        // Resolve using disk cache
        auto res2 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
        REQUIRE(res2 == results);
    }
}

// Test a single "post ISS object" request
template<typename Request>
static void
test_post_iss_request(
    Request const& req, bool introspected, bool use_dynamic = false)
{
    std::vector<Request> requests = {req};
    auto payload = use_dynamic ? value_to_msgpack_blob(dynamic("payload"))
                               : make_blob("payload");
    std::vector<blob> payloads = {payload};
    std::vector<std::string> results = {"def"};

    test_post_iss_requests_parallel(requests, payloads, results, introspected);
}

TEST_CASE("ISS POST - blob, uncached", "[requests]")
{
    test_post_iss_request(
        make_post_iss_request_constant<caching_level_type::none>(), false);
}

TEST_CASE("ISS POST - blob, memory cached", "[requests]")
{
    test_post_iss_request(
        make_post_iss_request_request<caching_level_type::memory>(), true);
}

TEST_CASE("ISS POST - blob, fully cached", "[requests]")
{
    test_post_iss_request(
        make_post_iss_request_request<caching_level_type::full>(), false);
}

TEST_CASE("ISS POST - dynamic, uncached", "[requests]")
{
    test_post_iss_request(
        make_post_iss_request_dynamic<caching_level_type::none>(),
        false,
        true);
}

TEST_CASE("ISS POST - fully cached, parallel", "[requests]")
{
    constexpr caching_level_type level = caching_level_type::full;
    using Req = decltype(make_post_iss_request_request<level>());
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

        requests.emplace_back(make_post_iss_request_request<level>(payload));

        std::ostringstream ss_result;
        ss_result << "result_" << req_id;
        auto result = ss_result.str();
        results.emplace_back(result);
    }

    test_post_iss_requests_parallel(requests, payloads, results);
}

static void
test_serialize_post_iss(auto const& req, std::string const& filename)
{
    // For the type-erased requests, req_type is
    // thinknode_request_erased<caching_level_type::full, std::string>.
    // For the container-based requests, it's something horrible.
    using req_type = std::remove_cvref_t<decltype(req)>;
    auto deserialize = [](cereal::JSONInputArchive& iarchive) {
        req_type req1;
        req1.load(iarchive);
        return req1;
    };
    auto test_request
        = [](auto const& req1) { test_post_iss_request(req1, false); };
    test_serialize_thinknode_request(req, deserialize, test_request, filename);
}

TEST_CASE("ISS POST serialization - container, blob", "[cereal]")
{
    auto req{make_post_iss_request_request<caching_level_type::full>()};
    test_serialize_post_iss(req, "iss_post_container_blob.json");
}

TEST_CASE("ISS POST serialization - erased, blob", "[cereal]")
{
    auto req{
        make_post_iss_request_erased_constant<caching_level_type::full>()};
    test_serialize_post_iss(req, "iss_post_erased_blob.json");
}

TEST_CASE("ISS POST serialization - function, blob", "[cereal]")
{
    auto req{rq_post_iss_object_func<caching_level_type::full>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"))};
    auto deserialize = [](cereal::JSONInputArchive& iarchive) {
        using Props = request_props<caching_level_type::full, true, true>;
        return function_request_erased<std::string, Props>(iarchive);
    };
    auto test_request
        = [](auto const& req1) { test_post_iss_request(req1, false); };
    test_serialize_thinknode_request(
        req, deserialize, test_request, "iss_post_func_blob.json");
}

TEST_CASE("ISS POST serialization - erased, inner request", "[cereal]")
{
    auto req{make_post_iss_request_erased_request<caching_level_type::full>()};
    test_serialize_post_iss(req, "iss_post_erased_inner_request.json");
}

template<typename Request>
static void
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
        service.inner_reset_memory_cache(
            immutable_cache_config{0x40'00'00'00});

        // Resolve using disk cache
        auto res2 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
        REQUIRE(res2 == results);
    }
}

template<typename Request>
static void
test_retrieve_immutable_object_req(Request const& req)
{
    std::vector<Request> requests = {req};
    std::vector<std::string> object_ids = {"abc"};
    std::vector<std::string> responses = {"payload"};

    test_retrieve_immutable_object_parallel(requests, object_ids, responses);
}

static void
test_retrieve_immutable_object(auto create_req)
{
    string api_url{"https://mgh.thinknode.io/api/v1.0"};
    string context_id{"123"};
    string immutable_id{"abc"};
    auto req{create_req(api_url, context_id, immutable_id)};
    test_retrieve_immutable_object_req(req);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - class, fully cached", "[requests]")
{
    // Using my_retrieve_immutable_object_request_base and mixin
    test_retrieve_immutable_object(
        rq_retrieve_immutable_object<caching_level_type::full>);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - function, fully cached", "[requests]")
{
    // Using function_request_erased
    test_retrieve_immutable_object(rq_retrieve_immutable_object_func<
                                   caching_level_type::full,
                                   std::string>);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - fully cached, parallel", "[requests]")
{
    constexpr caching_level_type level = caching_level_type::full;
    using Req = thinknode_request_erased<level, blob>;
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

        requests.push_back(rq_retrieve_immutable_object<level>(
            "https://mgh.thinknode.io/api/v1.0", "123", object_id));

        std::ostringstream ss_response;
        ss_response << "payload_" << req_id;
        auto response = ss_response.str();
        responses.push_back(response);
    }
    test_retrieve_immutable_object_parallel(requests, object_ids, responses);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT serialization - class", "[cereal]")
{
    auto req{rq_retrieve_immutable_object<caching_level_type::full>(
        "https://mgh.thinknode.io/api/v1.0", "123", "abc")};
    auto deserialize = [](cereal::JSONInputArchive& iarchive) {
        thinknode_request_erased<caching_level_type::full, blob> req1;
        req1.load(iarchive);
        return req1;
    };
    test_serialize_thinknode_request(
        req,
        deserialize,
        test_retrieve_immutable_object_req<decltype(req)>,
        "retrieve_immutable_class.json");
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT serialization - function", "[cereal]")
{
    using namespace std::string_literals;
    auto req{rq_retrieve_immutable_object_func<caching_level_type::full>(
        "https://mgh.thinknode.io/api/v1.0", "123", "abc"s)};
    auto deserialize = [](cereal::JSONInputArchive& iarchive) {
        using Props = request_props<caching_level_type::full, true, true>;
        return function_request_erased<blob, Props>(iarchive);
    };
    test_serialize_thinknode_request(
        req,
        deserialize,
        test_retrieve_immutable_object_req<decltype(req)>,
        "retrieve_immutable_func.json");
}

TEST_CASE(
    "RESOLVE ISS OBJECT TO IMMUTABLE serialization - function", "[cereal]")
{
    using value_type = std::string;
    using namespace std::string_literals;
    auto req{rq_resolve_iss_object_to_immutable_func<caching_level_type::full>(
        "https://mgh.thinknode.io/api/v1.0", "123", "abc"s, true)};
    auto deserialize = [](cereal::JSONInputArchive& iarchive) {
        using Props = request_props<caching_level_type::full, true, true>;
        return function_request_erased<value_type, Props>(iarchive);
    };
    auto test_request = [](auto const& req1) {};
    test_serialize_thinknode_request(
        req,
        deserialize,
        test_request,
        "resolve_iss_object_to_immutable_func.json");
}

// TODO make this work
#if 0
TEST_CASE("Composite request serialization - function", "[X]")
{
    using value_type = blob;
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
    auto req2{rq_retrieve_immutable_object_func<level>(
        api_url, context_id, req1)};
    auto deserialize = [](cereal::JSONInputArchive& iarchive) {
        using Props = request_props<level, true, true>;
        return function_request_erased<value_type, Props>(iarchive);
    };
    auto test_request
        = [](auto const& req1) { };
    test_serialize_thinknode_request(
        req2,
        deserialize,
        test_request,
        "composite_func.json");
}
#endif
