#include <cppcoro/sync_wait.hpp>

#include "test_session.h"
#include <cradle/external_api.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("post_iss_object", "[external]")
{
    const std::string context_id{"123"};
    const blob my_blob{make_blob("my_blob_value")};
    auto test_session = make_external_test_session();
    auto& api_session = test_session.api_session();
    auto& mock_http = test_session.enable_http_mocking();
    mock_http.set_script(
        {{make_http_request(
              http_request_method::POST,
              "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"},
               {"Content-Type", "application/octet-stream"}},
              my_blob),
          make_http_200_response(R"({ "id": "my_object_id" })")}});

    // Initial request; responses should come via (mock) HTTP
    auto result0 = cppcoro::sync_wait(cradle::external::post_iss_object(
        api_session, context_id, "string", my_blob));
    REQUIRE(result0 == "my_object_id");

    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // Same request; responses should come from the cache
    auto result1 = cppcoro::sync_wait(cradle::external::post_iss_object(
        api_session, context_id, "string", my_blob));
    REQUIRE(result1 == "my_object_id");
}
