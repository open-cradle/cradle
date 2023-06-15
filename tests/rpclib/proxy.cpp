#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/remote/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/registry.h>

#include "../support/inner_service.h"

using namespace cradle;

TEST_CASE("client name", "[rpclib]")
{
    auto client = ensure_rpclib_service();

    REQUIRE(client->name() == "rpclib");
}

TEST_CASE("send mock_http message", "[rpclib]")
{
    auto client = ensure_rpclib_service();

    REQUIRE_NOTHROW(client->mock_http("mock response"));
}

TEST_CASE("ping message", "[rpclib]")
{
    auto client = ensure_rpclib_service();

    auto git_version = client->ping();

    REQUIRE(git_version.size() > 0);
}

static void
test_make_some_blob(bool shared)
{
    constexpr auto caching_level{caching_level_type::full};
    constexpr auto remotely{true};
    std::string proxy_name{"rpclib"};
    register_testing_seri_resolvers();
    ensure_rpclib_service();
    inner_resources service;
    init_test_inner_service(service);
    testing_request_context ctx{service, nullptr, remotely, proxy_name};

    auto req{rq_make_some_blob<caching_level>(10000, shared)};
    auto response = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(response.size() == 10000);
    REQUIRE(response.data()[0xff] == static_cast<std::byte>(0x55));
    REQUIRE(response.data()[9999] == static_cast<std::byte>(0x35));
}

TEST_CASE("resolve to a plain blob", "[rpclib]")
{
    test_make_some_blob(false);
}

TEST_CASE("resolve to a blob file", "[rpclib]")
{
    test_make_some_blob(true);
}

TEST_CASE("sending bad request", "[rpclib]")
{
    inner_resources service;
    init_test_inner_service(service);
    auto client = ensure_rpclib_service();
    service_config_map config_map{
        {remote_config_keys::DOMAIN_NAME, "bad domain"},
    };

    REQUIRE_THROWS_AS(
        client->resolve_sync(service_config{config_map}, "bad request"),
        remote_error);
}
