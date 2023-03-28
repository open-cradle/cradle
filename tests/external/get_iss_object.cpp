#include <cstring>

#include <cppcoro/sync_wait.hpp>

#include "test_session.h"
#include <cradle/external_api.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("get_iss_object", "[external]")
{
    const std::string context_id{"123"};
    const std::string blob_value{"my_blob_value"};
    auto test_session = make_external_test_session();
    auto& api_session = test_session.api_session();
    auto& mock_http = test_session.enable_http_mocking();
    mock_http.set_script(
        {{make_get_request(
              "https://mgh.thinknode.io/api/v1.0/iss/my_object_id/"
              "immutable?context=123&ignore_upgrades=false",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"}}),
          make_http_200_response(R"({ "id": "my_immutable_id" })")},
         {make_get_request(
              "https://mgh.thinknode.io/api/v1.0/iss/immutable/"
              "my_immutable_id?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/octet-stream"}}),
          make_http_200_response(blob_value.c_str())}});

    // Initial request; responses should come via (mock) HTTP
    auto blob0 = cppcoro::sync_wait(cradle::external::get_iss_object(
        api_session, context_id, "my_object_id"));

    REQUIRE(blob0.size() == blob_value.size());
    auto blob0_str = reinterpret_cast<const char*>(blob0.data());
    REQUIRE(strncmp(blob0_str, blob_value.c_str(), blob_value.size()) == 0);

    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // Same request; responses should come from the cache
    auto blob1 = cppcoro::sync_wait(cradle::external::get_iss_object(
        api_session, context_id, "my_object_id"));

    REQUIRE(blob1.size() == blob_value.size());
    auto blob1_str = reinterpret_cast<const char*>(blob1.data());
    REQUIRE(strncmp(blob1_str, blob_value.c_str(), blob_value.size()) == 0);
}
