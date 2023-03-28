#include <utility>

#include <cradle/external_api.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("start_session", "[external]")
{
    std::string json_config{"{}"};
    auto service = cradle::external::start_service(std::move(json_config));
    cradle::external::api_thinknode_session_config session_config{
        .api_url = "https://mgh.thinknode.io/api/v1.0",
        .access_token = "xyz",
    };
    REQUIRE_NOTHROW(cradle::external::start_session(service, session_config));
}
