#include <catch2/catch.hpp>

#include <cradle/typing/core/dynamic.h>
#include <cradle/typing/core/fmt_format.h>

using namespace cradle;

TEST_CASE("format dynamic", "[core][fmt]")
{
    REQUIRE(fmt::format("{}", dynamic(false)) == "false");
}
