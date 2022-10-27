#include <catch2/catch.hpp>

#include <cradle/inner/service/config_map_json.h>

using namespace cradle;

TEST_CASE("correct JSON config", "[encodings][config_map_json]")
{
    std::string json_text{
        R"(
        [
            { "key": "port", "value": 41071 },
            { "key": "open", "value": false },
            { "key": "disk_cache/directory", "value": "some_dir" }
        ]
        )"};
    service_config_map expected{
        {"port", 41071U},
        {"open", false},
        {"disk_cache/directory", "some_dir"}};
    REQUIRE(read_config_map_from_json(json_text) == expected);
}

TEST_CASE("corrupt JSON", "[encodings][config_map_json]")
{
    std::string json_text{
        R"(
        [
            { "key": "port", "value": 41071
        ]
        )"};
    REQUIRE_THROWS(read_config_map_from_json(json_text));
}

TEST_CASE("value with unsupported type", "[encodings][config_map_json]")
{
    std::string json_text{
        R"(
        [
            { "key": "port", "value": [] }
        ]
        )"};
    REQUIRE_THROWS(read_config_map_from_json(json_text));
}

TEST_CASE("object with bad number of entries", "[encodings][config_map_json]")
{
    std::string json_text{
        R"(
        [
            { "key": "port", "value": 41071, "extra": "extra" }
        ]
        )"};
    REQUIRE_THROWS(read_config_map_from_json(json_text));
}

TEST_CASE("object with missing key", "[encodings][config_map_json]")
{
    std::string json_text{
        R"(
        [
            { "no_key": "port", "value": 41071 }
        ]
        )"};
    REQUIRE_THROWS(read_config_map_from_json(json_text));
}

TEST_CASE("object with missing value", "[encodings][config_map_json]")
{
    std::string json_text{
        R"(
        [
            { "key": "port", "no_value": 41071 }
        ]
        )"};
    REQUIRE_THROWS(read_config_map_from_json(json_text));
}
