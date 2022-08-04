#include <cradle/thinknode/iss_req.h>

#include <cereal/types/string.hpp>
#include <cppcoro/sync_wait.hpp>
#include <spdlog/spdlog.h>

#include "../../inner/introspection/tasklet_testing.h"
#include "../../inner/support/concurrency_testing.h"
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/service/request.h>
#include <cradle/typing/encodings/msgpack.h>
#include <cradle/typing/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("ISS POST - blob, uncached", "[requests]")
{
    service_core service;
    init_test_service(service);

    auto& mock_http = enable_http_mocking(service);
    mock_http.set_script(
        {{make_http_request(
              http_request_method::POST,
              "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"},
               {"Content-Type", "application/octet-stream"}},
              make_blob("payload")),
          make_http_200_response("{ \"id\": \"def\" }")}});

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    thinknode_request_context ctx{service, session, nullptr};
    string context_id{"123"};

    auto req{rq_post_iss_object<caching_level_type::none>(
        session.api_url,
        context_id,
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"))};
    auto id = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(id == "def");
    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());
}

TEST_CASE("ISS POST - dynamic, uncached", "[requests]")
{
    service_core service;
    init_test_service(service);

    auto& mock_http = enable_http_mocking(service);
    mock_http.set_script(
        {{make_http_request(
              http_request_method::POST,
              "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"},
               {"Content-Type", "application/octet-stream"}},
              value_to_msgpack_blob(dynamic("payload"))),
          make_http_200_response("{ \"id\": \"def\" }")}});

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    thinknode_request_context ctx{service, session, nullptr};
    string context_id{"123"};

    auto req{rq_post_iss_object<caching_level_type::none>(
        session.api_url,
        context_id,
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        dynamic("payload"))};
    auto id = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(id == "def");
    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());
}

TEST_CASE("ISS POST - blob, memory cached", "[requests]")
{
    clean_tasklet_admin_fixture fixture;
    service_core service;
    init_test_service(service);

    auto& mock_http = enable_http_mocking(service);
    mock_http.set_script(
        {{make_http_request(
              http_request_method::POST,
              "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"},
               {"Content-Type", "application/octet-stream"}},
              make_blob("payload")),
          make_http_200_response("{ \"id\": \"def\" }")}});

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    auto tasklet = create_tasklet_tracker("my_pool", "my_title");
    thinknode_request_context ctx{service, session, tasklet};
    string context_id{"123"};

    auto make_blob_function
        = [](std::string const& payload) { return make_blob(payload); };
    auto make_blob_request = rq_function_erased<caching_level_type::memory>(
        make_blob_function, rq_value(std::string("payload")));

    auto req{rq_post_iss_object<caching_level_type::memory>(
        session.api_url,
        context_id,
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob_request)};

    // Resolve using task, storing result in memory cache (and disk cache)
    auto id0 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(id0 == "def");
    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // Resolve using memory cache
    auto id1 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(id1 == "def");

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

TEST_CASE("ISS POST - blob, fully cached", "[requests]")
{
    service_core service;
    init_test_service(service);

    auto& mock_http = enable_http_mocking(service);
    mock_http.set_script(
        {{make_http_request(
              http_request_method::POST,
              "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"},
               {"Content-Type", "application/octet-stream"}},
              make_blob("payload")),
          make_http_200_response("{ \"id\": \"def\" }")}});

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    auto tasklet = create_tasklet_tracker("my_pool", "my_title");
    thinknode_request_context ctx{service, session, tasklet};
    string context_id{"123"};

    auto make_blob_function
        = [](std::string const& payload) { return make_blob(payload); };
    auto make_blob_request = rq_function_erased_uuid<caching_level_type::full>(
        "uuid", make_blob_function, rq_value(std::string("payload")));

    auto req{rq_post_iss_object<caching_level_type::full>(
        session.api_url,
        context_id,
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob_request)};

    // Resolve using HTTP, storing result in both caches
    auto id0 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(id0 == "def");
    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // Resolve using memory cache
    auto id1 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(id1 == "def");

    sync_wait_write_disk_cache(service);
    service.inner_reset_memory_cache(immutable_cache_config{0x40'00'00'00});

    // Resolve using disk cache
    auto id2 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(id2 == "def");
}

void
test_retrieve_immutable_object(auto CreateReq)
{
    service_core service;
    init_test_service(service);
    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    auto tasklet = create_tasklet_tracker("my_pool", "my_title");
    thinknode_request_context ctx{service, session, tasklet};
    string context_id{"123"};
    string immutable_id{"abc"};
    auto expected{make_blob(std::string("payload"))};
    auto& mock_http = enable_http_mocking(service);
    mock_http.set_script(
        {{make_get_request(
              "https://mgh.thinknode.io/api/v1.0/iss/immutable/"
              "abc?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/octet-stream"}}),
          make_http_200_response("payload")}});
    auto req{CreateReq(session.api_url, context_id, immutable_id)};

    // Resolve using HTTP, storing result in both caches
    auto blob0 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(blob0 == expected);
    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // Resolve using memory cache
    auto blob1 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(blob1 == expected);

    sync_wait_write_disk_cache(service);
    service.inner_reset_memory_cache(immutable_cache_config{0x40'00'00'00});

    // Resolve using disk cache
    auto blob2 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(blob2 == expected);
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
    test_retrieve_immutable_object(
        rq_retrieve_immutable_object_func<caching_level_type::full>);
}
