#include <cppcoro/sync_wait.hpp>

#include "test_session.h"
#include <cradle/external_api.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("retrieve_calculation_request", "[external]")
{
    const std::string context_id{"123"};
    auto test_session = make_external_test_session();
    auto& api_session = test_session.api_session();
    auto& mock_http = test_session.enable_http_mocking();
    mock_http.set_script(
        {{make_get_request(
              "https://mgh.thinknode.io/api/v1.0/calc/"
              "61e53771010054e512d49dce6f3ebd8b?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"}}),
          make_http_200_response(
              R"({ "function": { "account": "abc", "app": "math", "name": "echo", "args": [ { "value": "42" } ] } })")}});

    // Initial request; responses should come via (mock) HTTP
    cppcoro::sync_wait(cradle::external::retrieve_calculation_request(
        api_session, context_id, "61e53771010054e512d49dce6f3ebd8b"));

    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // Same request; responses should come from the cache
    cppcoro::sync_wait(cradle::external::retrieve_calculation_request(
        api_session, context_id, "61e53771010054e512d49dce6f3ebd8b"));
}
