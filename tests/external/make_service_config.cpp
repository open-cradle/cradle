// Tests for internal functionality not exposed in the external API

#include <cradle/external/external_api_impl.h>
#include <cradle/external_api.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("make_service_config default settings", "[external]")
{
    cradle::external::api_service_config api_config{};
    auto impl_config = cradle::external::make_service_config(api_config);

    REQUIRE(!impl_config.immutable_cache);
    REQUIRE(!impl_config.disk_cache);
    REQUIRE(!impl_config.request_concurrency);
    REQUIRE(!impl_config.compute_concurrency);
    REQUIRE(!impl_config.http_concurrency);
}

TEST_CASE("make_service_config all settings", "[external]")
{
    cradle::external::api_service_config api_config{
        .memory_cache_unused_size_limit = 100,
        .disk_cache_directory = "/some/path",
        .disk_cache_size_limit = 200,
        .request_concurrency = 3,
        .compute_concurrency = 4,
        .http_concurrency = 5,
    };
    auto impl_config = cradle::external::make_service_config(api_config);

    REQUIRE(impl_config.immutable_cache);
    REQUIRE(impl_config.immutable_cache->unused_size_limit == 100);
    REQUIRE(impl_config.disk_cache);
    REQUIRE(impl_config.disk_cache->directory == "/some/path");
    REQUIRE(impl_config.disk_cache->size_limit == 200);
    REQUIRE(*impl_config.request_concurrency == 3);
    REQUIRE(*impl_config.compute_concurrency == 4);
    REQUIRE(*impl_config.http_concurrency == 5);
}

TEST_CASE("make_service_config - disk cache limit only", "[external]")
{
    cradle::external::api_service_config api_config{
        .memory_cache_unused_size_limit{},
        .disk_cache_directory{},
        .disk_cache_size_limit{400},
        .request_concurrency{},
        .compute_concurrency{},
        .http_concurrency{}};
    auto impl_config = cradle::external::make_service_config(api_config);

    REQUIRE(impl_config.disk_cache);
    REQUIRE(!impl_config.disk_cache->directory);
    REQUIRE(impl_config.disk_cache->size_limit == 400);
}

TEST_CASE("make_service_config - disk cache directory only", "[external]")
{
    cradle::external::api_service_config api_config{
        .memory_cache_unused_size_limit{},
        .disk_cache_directory{"/some/path"},
        .disk_cache_size_limit{},
        .request_concurrency{},
        .compute_concurrency{},
        .http_concurrency{}};

    REQUIRE_THROWS_WITH(
        cradle::external::make_service_config(api_config),
        Catch::Matchers::Contains("config.disk_cache_directory given but not "
                                  "config.disk_cache_size_limit"));
}
