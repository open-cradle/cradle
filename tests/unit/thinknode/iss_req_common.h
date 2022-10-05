#ifndef CRADLE_TESTS_UNIT_THINKNODE_ISS_REQ_COMMON_H
#define CRADLE_TESTS_UNIT_THINKNODE_ISS_REQ_COMMON_H

#include <fstream>
#include <string>
#include <vector>

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>

#include <cradle/typing/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>
#include <cradle/typing/encodings/msgpack.h>

namespace cradle {

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
void
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
        service.inner_reset_memory_cache(
            immutable_cache_config{0x40'00'00'00});

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

inline void
test_retrieve_immutable_object(auto create_req, auto immutable_id)
{
    string api_url{"https://mgh.thinknode.io/api/v1.0"};
    string context_id{"123"};
    auto req{create_req(api_url, context_id, immutable_id)};
    test_retrieve_immutable_object_req(req);
}

} // namespace cradle

#endif
