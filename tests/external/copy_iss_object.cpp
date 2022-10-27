#include <cppcoro/sync_wait.hpp>

#include "test_session.h"
#include <cradle/external_api.h>
#include <cradle/typing/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("copy_iss_object", "[external]")
{
    const std::string source_context_id{"123"};
    const std::string destination_context_id{"456"};
    auto metadata = std::map<string, string>(
        {{"Access-Control-Allow-Origin", "*"},
         {"Cache-Control", "max-age=60"},
         {"Thinknode-Type", "string"}});
    auto test_session = make_external_test_session();
    auto& api_session = test_session.api_session();
    auto& mock_http = test_session.enable_http_mocking();
    mock_http.set_script(
        {{make_get_request(
              "https://mgh.thinknode.io/api/v1.0/iam/contexts/123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"}}),
          make_http_200_response(
              R"({ "bucket": "my_bucket_id", "contents": [] })")},
         // Shallow copy
         {make_http_request(
              http_request_method::POST,
              "https://mgh.thinknode.io/api/v1.0/iss/my_object_id/buckets/"
              "my_bucket_id?context=456",
              {{"Authorization", "Bearer xyz"}},
              blob()),
          make_http_200_response("")},
         // Get the metadata to check if the data type implies a deep copy;
         // it's a string, so no further requests are needed.
         {make_http_request(
              http_request_method::HEAD,
              "https://mgh.thinknode.io/api/v1.0/iss/my_object_id?context=123",
              {{"Authorization", "Bearer xyz"}},
              blob()),
          make_http_response(200, metadata, blob())}});

    // Initial request; responses should come via (mock) HTTP
    cppcoro::sync_wait(cradle::external::copy_iss_object(
        api_session,
        source_context_id,
        destination_context_id,
        "my_object_id"));

    REQUIRE(mock_http.is_complete());
    // The POST and HEAD requests come from different threads
    // REQUIRE(mock_http.is_in_order());

    // Same request; responses should come from the cache
    cppcoro::sync_wait(cradle::external::copy_iss_object(
        api_session,
        source_context_id,
        destination_context_id,
        "my_object_id"));
}
