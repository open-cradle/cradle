#include <cppcoro/sync_wait.hpp>

#include <cradle/external_api.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/typing/encodings/msgpack.h>
#include <cradle/typing/utilities/testing.h>
#include <cradle/websocket/calculations.h>

#include "test_session.h"

using namespace cradle;

TEST_CASE("resolve_calc_to_iss_object", "[external]")
{
    std::string const context_id{"123"};
    blob const expected_result = value_to_msgpack_blob(dynamic(2.0));
    auto test_session = make_external_test_session();
    auto& api_session = test_session.api_session();
    auto& mock_http = test_session.enable_http_mocking();
    mock_http.set_script(
        {{make_http_request(
              http_request_method::POST,
              "https://mgh.thinknode.io/api/v1.0/iss/dynamic?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"},
               {"Content-Type", "application/octet-stream"}},
              expected_result),
          make_http_200_response(R"({ "id": "my_object_id" })")}});

    auto test_calc
        = make_calculation_request_with_lambda(make_lambda_calculation(
            make_function([](dynamic_array args, tasklet_tracker*) {
                return dynamic(
                    cast<double>(args.at(0)) + cast<double>(args.at(1)));
            }),
            {make_calculation_request_with_value(dynamic(1.0)),
             make_calculation_request_with_value(dynamic(1.0))}));

    // Initial request; responses should come via (mock) HTTP
    auto result0
        = cppcoro::sync_wait(cradle::external::resolve_calc_to_iss_object(
            api_session, context_id, test_calc));
    REQUIRE(result0 == "my_object_id");

    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // Same request; responses should come from the cache
    auto result1
        = cppcoro::sync_wait(cradle::external::resolve_calc_to_iss_object(
            api_session, context_id, test_calc));
    REQUIRE(result1 == "my_object_id");
}
