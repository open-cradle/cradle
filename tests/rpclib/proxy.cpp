#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/remote/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>
#include <cradle/rpclib/client/proxy.h>

#include "../support/inner_service.h"

using namespace cradle;

TEST_CASE("client name", "[rpclib]")
{
    std::string proxy_name{"rpclib"};
    auto resources{make_inner_test_resources(proxy_name)};
    auto& client{resources->get_proxy(proxy_name)};

    REQUIRE(client.name() == "rpclib");
}

TEST_CASE("alternate logger for client", "[rpclib]")
{
    std::string proxy_name{"rpclib"};
    auto config{make_inner_tests_config()};
    auto resources{std::make_unique<inner_resources>(config)};
    resources->set_secondary_cache(std::make_unique<local_disk_cache>(config));
    auto logger{ensure_logger("alternate")};
    resources->register_proxy(
        std::make_unique<rpclib_client>(resources->config(), logger));
    auto& client{resources->get_proxy(proxy_name)};

    REQUIRE(&client.get_logger() == logger.get());
}

TEST_CASE("send mock_http message", "[rpclib]")
{
    std::string proxy_name{"rpclib"};
    auto resources{make_inner_test_resources(proxy_name)};
    auto& client{resources->get_proxy(proxy_name)};

    REQUIRE_NOTHROW(client.mock_http("mock response"));
}

TEST_CASE("ping message", "[rpclib]")
{
    std::string proxy_name{"rpclib"};
    auto resources{make_inner_test_resources(proxy_name)};
    auto& client{
        static_cast<rpclib_client&>(resources->get_proxy(proxy_name))};

    auto git_version = client.ping();

    REQUIRE(git_version.size() > 0);
}

static void
test_make_some_blob(bool use_shared_memory)
{
    constexpr auto caching_level{caching_level_type::full};
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto* tasklet{create_tasklet_tracker("test", "make_some_blob")};
    testing_request_context ctx{*resources, tasklet, proxy_name};

    auto req{rq_make_some_blob<caching_level>(10000, use_shared_memory)};
    auto response = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(response.size() == 10000);
    REQUIRE(response.data()[0xff] == static_cast<std::byte>(0x55));
    REQUIRE(response.data()[9999] == static_cast<std::byte>(0x35));
}

TEST_CASE("resolve to a plain blob", "[rpclib]")
{
    introspection_set_capturing_enabled(true);
    test_make_some_blob(false);
}

TEST_CASE("resolve to a blob file", "[rpclib]")
{
    test_make_some_blob(true);
}

TEST_CASE("sending bad request", "[rpclib]")
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& client{resources->get_proxy(proxy_name)};
    service_config_map config_map{
        {remote_config_keys::DOMAIN_NAME, "bad domain"},
    };

    REQUIRE_THROWS_AS(
        client.resolve_sync(service_config{config_map}, "bad request"),
        remote_error);
}

TEST_CASE("rpclib protocol mismatch", "[rpclib]")
{
    std::string proxy_name{"rpclib"};
    auto resources{make_inner_test_resources(proxy_name)};
    auto& client{
        static_cast<rpclib_client&>(resources->get_proxy(proxy_name))};

    REQUIRE_THROWS_AS(
        client.verify_rpclib_protocol("incompatible"), remote_error);
}
