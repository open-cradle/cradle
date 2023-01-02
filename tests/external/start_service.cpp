#include <string>
#include <utility>

#include <cradle/external_api.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("start_service", "[external]")
{
    std::string json_config{"{}"};
    REQUIRE_NOTHROW(cradle::external::start_service(std::move(json_config)));
}
