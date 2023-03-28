#include <cppcoro/sync_wait.hpp>

#include "test_session.h"
#include <cradle/external_api.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("get_iss_object_metadata", "[external]")
{
    const std::string context_id{"123"};
    auto metadata = std::map<string, string>(
        {{"Access-Control-Allow-Origin", "*"},
         {"Cache-Control", "max-age=60"}});
    auto test_session = make_external_test_session();
    auto& api_session = test_session.api_session();
    auto& mock_http = test_session.enable_http_mocking();
    mock_http.set_script(
        {{make_http_request(
              http_request_method::HEAD,
              "https://mgh.thinknode.io/api/v1.0/iss/my_object_id?context=123",
              {{"Authorization", "Bearer xyz"}},
              blob()),
          make_http_response(200, metadata, blob())}});

    // Initial request; responses should come via (mock) HTTP
    auto result0
        = cppcoro::sync_wait(cradle::external::get_iss_object_metadata(
            api_session, context_id, "my_object_id"));
    REQUIRE(result0 == metadata);

    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // Same request; responses should come from the cache
    auto result1
        = cppcoro::sync_wait(cradle::external::get_iss_object_metadata(
            api_session, context_id, "my_object_id"));
    REQUIRE(result1 == metadata);
}
