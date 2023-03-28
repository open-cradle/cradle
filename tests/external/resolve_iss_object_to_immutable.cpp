#include <cppcoro/sync_wait.hpp>

#include "test_session.h"
#include <cradle/external_api.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("resolve_iss_object_to_immutable", "[external]")
{
    const std::string context_id{"123"};
    auto test_session = make_external_test_session();
    auto& api_session = test_session.api_session();
    auto& mock_http = test_session.enable_http_mocking();
    mock_http.set_script(
        {{make_get_request(
              "https://mgh.thinknode.io/api/v1.0/iss/my_object_id/"
              "immutable?context=123&ignore_upgrades=false",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"}}),
          make_http_200_response(R"({ "id": "my_immutable_id" })")}});

    // Initial request; responses should come via (mock) HTTP
    auto result0
        = cppcoro::sync_wait(cradle::external::resolve_iss_object_to_immutable(
            api_session, context_id, "my_object_id"));
    REQUIRE(result0 == "my_immutable_id");

    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // Same request; responses should come from the cache
    auto result1
        = cppcoro::sync_wait(cradle::external::resolve_iss_object_to_immutable(
            api_session, context_id, "my_object_id"));
    REQUIRE(result1 == "my_immutable_id");
}
