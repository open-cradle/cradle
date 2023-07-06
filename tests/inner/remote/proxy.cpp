#include <stdexcept>
#include <string>

#include <catch2/catch.hpp>

#include <cradle/inner/remote/proxy.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][remote][proxy]";

} // namespace

TEST_CASE("construct remote_error, one arg", tag)
{
    auto exc{remote_error("argX")};

    std::string what(exc.what());

    REQUIRE(what.find("argX") != std::string::npos);
}

TEST_CASE("construct remote_error, two args", tag)
{
    auto exc{remote_error("argX", "argY")};

    std::string what(exc.what());

    REQUIRE(what.find("argX") != std::string::npos);
    REQUIRE(what.find("argY") != std::string::npos);
}
