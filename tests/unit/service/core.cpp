#include <cradle/thinknode/service/core.h>

#include <cppcoro/sync_wait.hpp>

#include "../../support/thinknode.h"
#include <cradle/typing/io/http_requests.hpp>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("HTTP requests", "[service][core]")
{
    service_core core;
    init_test_service(core);

    auto async_response = async_http_request(
        core,
        make_get_request(
            "https://postman-echo.com/get?color=navy", http_header_list()));

    auto response = cppcoro::sync_wait(async_response);
    REQUIRE(response.status_code == 200);
    auto body = parse_json_response(response);
    REQUIRE(
        get_field(cast<dynamic_map>(body), "args")
        == dynamic({{"color", "navy"}}));
}
