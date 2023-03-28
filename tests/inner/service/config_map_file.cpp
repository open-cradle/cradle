#include <catch2/catch.hpp>

#include <cradle/inner/service/config_map_file.h>

using namespace cradle;

static char const tag[] = "[service][config_map_file]";

TEST_CASE("is_toml_file", tag)
{
    REQUIRE(is_toml_file("bla.toml") == true);
    REQUIRE(is_toml_file("bla.TOML") == true);
    REQUIRE(is_toml_file("bla.json") == false);
    REQUIRE(is_toml_file("blatoml") == false);
    REQUIRE(is_toml_file("bla.tomlx") == false);
}
