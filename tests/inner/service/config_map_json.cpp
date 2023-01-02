#include <catch2/catch.hpp>

#include <cradle/inner/service/config_map_json.h>

using namespace cradle;

TEST_CASE("correct JSON config", "[encodings][config_map_json]")
{
    std::string json_text{
        R"(
        {
            "port": 41071,
            "open": false,
            "disk_cache/directory": "some_dir"
        }
        )"};
    service_config_map expected{
        {"port", 41071U},
        {"open", false},
        {"disk_cache/directory", "some_dir"}};
    REQUIRE(read_config_map_from_json(json_text) == expected);
}

TEST_CASE("Docker JSON config", "[encodings][config_map_json]")
{
    std::string json_text{
        R"(
        {
            "disk_cache": {
                "directory": "/var/cache/cradle",
                "size_limit": 6000000000
            },
            "open": true
        }
        )"};
    service_config_map expected{
        {"disk_cache/directory", "/var/cache/cradle"},
        {"disk_cache/size_limit", 6000000000UL},
        {"open", true}};
    REQUIRE(read_config_map_from_json(json_text) == expected);
}

TEST_CASE("corrupt JSON", "[encodings][config_map_json]")
{
    std::string json_text{
        R"(
        {
            "port": 41071
        ]
        )"};
    REQUIRE_THROWS(read_config_map_from_json(json_text));
}

TEST_CASE("value with unsupported type", "[encodings][config_map_json]")
{
    std::string json_text{
        R"(
        {
            "port": []
        }
        )"};
    REQUIRE_THROWS(read_config_map_from_json(json_text));
}
