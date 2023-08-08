#include <catch2/catch.hpp>

#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/id.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][core][get_unique_string]";

} // namespace

TEST_CASE("get_unique_string", tag)
{
    auto id0 = make_captured_id(0);
    auto id1 = make_captured_id(1);

    std::string s0{get_unique_string(*id0)};
    std::string s1{get_unique_string(*id1)};

    REQUIRE(s0 != "");
    REQUIRE(s0.size() == 64);
    REQUIRE(s1 != "");
    REQUIRE(s1.size() == 64);
    REQUIRE(s0 != s1);
}
