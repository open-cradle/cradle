#include <cppcoro/sync_wait.hpp>

#include <cradle/external_api.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/typing/utilities/testing.h>
#include <cradle/websocket/calculations.h>

#include "test_session.h"

using namespace cradle;

TEST_CASE("resolve_calc_to_value", "[external]")
{
    std::string const context_id{"123"};
    auto test_session = make_external_test_session();
    auto& api_session = test_session.api_session();

    auto test_calc
        = make_calculation_request_with_lambda(make_lambda_calculation(
            make_function([](dynamic_array args, tasklet_tracker*) {
                return dynamic(
                    cast<double>(args.at(0)) + cast<double>(args.at(1)));
            }),
            {make_calculation_request_with_value(dynamic(1.0)),
             make_calculation_request_with_value(dynamic(1.0))}));

    // Initial request; responses should be computed directly
    auto result0 = cppcoro::sync_wait(cradle::external::resolve_calc_to_value(
        api_session, context_id, test_calc));
    REQUIRE(result0 == dynamic(2.0));

    // Same request; responses should come from the cache
    auto result1 = cppcoro::sync_wait(cradle::external::resolve_calc_to_value(
        api_session, context_id, test_calc));
    REQUIRE(result1 == dynamic(2.0));
}
