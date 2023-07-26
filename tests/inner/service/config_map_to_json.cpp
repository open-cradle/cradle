#include <stdexcept>

#include <catch2/catch.hpp>

#include <cradle/inner/service/config_map_to_json.h>

using namespace cradle;

static char const tag[] = "[service][config_map_to_json]";

TEST_CASE("service_config_map to JSON - good", tag)
{
    service_config_map const sample_map{
        {"a", true},
        {"b", 1U},
        {"d/c", false},
        {"d/b", 2U},
        {"d/a", "Y"},
        {"c", "X"},
    };
    REQUIRE(
        write_config_map_to_json(sample_map)
        == R"({"a":true,"b":1,"c":"X","d":{"a":"Y","b":2,"c":false}})");
}

TEST_CASE("service_config_map to JSON - key too deep", tag)
{
    service_config_map const sample_map{
        {"a/b/c", true},
    };
    REQUIRE_THROWS_AS(
        write_config_map_to_json(sample_map), std::invalid_argument);
}
