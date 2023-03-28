#include <catch2/catch.hpp>

#include <cradle/inner/service/config_map_toml.h>

using namespace cradle;

static char const tag[] = "[service][config_map_toml]";

TEST_CASE("correct TOML config", tag)
{
    std::string toml_text{
        R"(
        port = 41071
        open = false

        [disk_cache]
        directory = "some_dir"
        )"};
    service_config_map expected{
        {"port", 41071U},
        {"open", false},
        {"disk_cache/directory", "some_dir"}};
    REQUIRE(read_config_map_from_toml(toml_text) == expected);
}

TEST_CASE("Docker TOML config", tag)
{
    std::string toml_text{
        R"(
        open = true

        [disk_cache]
        directory = "/var/cache/cradle"
        size_limit = 6000000000
        )"};
    service_config_map expected{
        {"disk_cache/directory", "/var/cache/cradle"},
        {"disk_cache/size_limit", 6000000000UL},
        {"open", true}};
    REQUIRE(read_config_map_from_toml(toml_text) == expected);
}

TEST_CASE("corrupt TOML", tag)
{
    std::string toml_text{
        R"(
        unquoted = /var/cache/cradle
        )"};
    REQUIRE_THROWS(read_config_map_from_toml(toml_text));
}

TEST_CASE("TOML value with unsupported type", tag)
{
    std::string toml_text{
        R"(
        array = [1, 2]
        )"};
    REQUIRE_THROWS(read_config_map_from_toml(toml_text));
}

TEST_CASE("TOML value is signed integer", tag)
{
    std::string toml_text{
        R"(
        negative = -1
        )"};
    REQUIRE_THROWS(read_config_map_from_toml(toml_text));
}

TEST_CASE("Reading TOML from non-existing file", tag)
{
    std::string path{"/no/such/file.toml"};
    REQUIRE_THROWS(read_config_map_from_toml_file(path));
}
