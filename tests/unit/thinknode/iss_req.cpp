#include <cradle/thinknode/iss_req.h>

#include <cppcoro/sync_wait.hpp>

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

    auto req{rq_post_iss_object(
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

    auto req{rq_post_iss_object(
        context_id,
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        dynamic("payload"))};
    auto id = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(id == "def");
    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());
}
