#include <string>

#include <catch2/catch.hpp>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>

using namespace cradle;

namespace {

char const tag[] = "[local_disk_cache]";

std::string tests_cache_dir{"tests_cache"};
service_config_map const inner_config_map{
    {generic_config_keys::TESTING, true},
    {local_disk_cache_config_keys::DIRECTORY, tests_cache_dir},
    {local_disk_cache_config_keys::SIZE_LIMIT, 0x40'00'00'00U},
    {local_disk_cache_config_keys::NUM_THREADS_READ_POOL, 2U},
    {local_disk_cache_config_keys::NUM_THREADS_WRITE_POOL, 2U},
    {local_disk_cache_config_keys::START_EMPTY, true},
};

service_config
create_config()
{
    return service_config{inner_config_map};
}

} // namespace

TEST_CASE("read/write raw value", tag)
{
    local_disk_cache cache{create_config()};
    std::string key{"some_key"};
    auto written_value{make_string_literal_blob("written value")};

    cache.write_raw_value(key, written_value);

    auto read_value{cache.read_raw_value(key)};
    REQUIRE(read_value);
    REQUIRE(*read_value == written_value);
}

TEST_CASE("read non-existent raw value", tag)
{
    local_disk_cache cache{create_config()};
    std::string written_key{"written_key"};
    auto written_value{make_string_literal_blob("written value")};
    std::string read_key{"read_key"};

    auto read_value0{cache.read_raw_value(read_key)};
    REQUIRE(!read_value0);

    cache.write_raw_value(written_key, written_value);

    auto read_value1{cache.read_raw_value(read_key)};
    REQUIRE(!read_value1);
}
