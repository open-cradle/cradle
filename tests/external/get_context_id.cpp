#include <cppcoro/sync_wait.hpp>

#include "test_session.h"
#include <cradle/external_api.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("get_context_id", "[external]")
{
    auto test_session = make_external_test_session();
    auto& api_session = test_session.api_session();
    auto& mock_http = test_session.enable_http_mocking();
    mock_http.set_script(
        {{make_get_request(
              "https://mgh.thinknode.io/api/v1.0/iam/realms/my_realm/context",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"}}),
          make_http_200_response(R"({ "id": "my_context_id" })")}});

    auto context_id = cppcoro::sync_wait(
        cradle::external::get_context_id(api_session, "my_realm"));
    REQUIRE(context_id == "my_context_id");

    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // get_context_id requests are not cached
    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(
            cradle::external::get_context_id(api_session, "my_realm")),
        Catch::Matchers::Contains("unrecognized mock HTTP request"));
}
