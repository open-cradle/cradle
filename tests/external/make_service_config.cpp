// Tests for internal functionality not exposed in the external API

#include <cradle/external/external_api_impl.h>
#include <cradle/external_api.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("make_service_config default settings", "[external]")
{
    cradle::external::api_service_config api_config{};
    auto svc_config = cradle::external::make_service_config(api_config);

    REQUIRE(!svc_config.contains("memory_cache/unused_size_limit"));
    REQUIRE(!svc_config.contains("disk_cache/directory"));
    REQUIRE(!svc_config.contains("disk_cache/size_limit"));
    REQUIRE(!svc_config.contains("request_concurrency"));
    REQUIRE(!svc_config.contains("compute_concurrency"));
    REQUIRE(!svc_config.contains("http_concurrency"));
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
    auto svc_config = cradle::external::make_service_config(api_config);

    REQUIRE(
        svc_config.get_mandatory_number("memory_cache/unused_size_limit")
        == 100);
    REQUIRE(
        svc_config.get_mandatory_string("disk_cache/directory")
        == "/some/path");
    REQUIRE(svc_config.get_mandatory_number("disk_cache/size_limit") == 200);
    REQUIRE(svc_config.get_mandatory_number("request_concurrency") == 3);
    REQUIRE(svc_config.get_mandatory_number("compute_concurrency") == 4);
    REQUIRE(svc_config.get_mandatory_number("http_concurrency") == 5);
}
