#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/registry.h>

#include "../support/inner_service.h"

using namespace cradle;

TEST_CASE("construct rpclib_error, one arg", "[rpclib]")
{
    auto exc{rpclib_error("argX")};

    std::string what(exc.what());

    REQUIRE(what.find("argX") != std::string::npos);
}

TEST_CASE("construct rpclib_error, two args", "[rpclib]")
{
    auto exc{rpclib_error("argX", "argY")};

    std::string what(exc.what());

    REQUIRE(what.find("argX") != std::string::npos);
    REQUIRE(what.find("argY") != std::string::npos);
}

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
    testing_request_context ctx{service, nullptr, remotely};
    ctx.proxy_name(proxy_name);

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
    constexpr auto remotely{true};
    std::string proxy_name{"rpclib"};
    ensure_rpclib_service();
    inner_resources service;
    init_test_inner_service(service);
    testing_request_context ctx{service, nullptr, remotely};
    ctx.proxy_name(proxy_name);

    auto client = ensure_rpclib_service();
    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(client->resolve_request(ctx, "bad request")),
        rpclib_error);
}
