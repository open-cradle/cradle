#include <catch2/catch.hpp>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>

#include "../../support/inner_service.h"

using namespace cradle;

TEST_CASE("get_resources()", "[testing][context]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr};

    REQUIRE(&ctx.get_resources() == &resources);
    REQUIRE(&ctx.get_resources().memory_cache() == &resources.memory_cache());
}

TEST_CASE("remotely(), default", "[testing][context]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr};

    REQUIRE(!ctx.remotely());
}

TEST_CASE("remotely(), set", "[testing][context]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr, true};

    REQUIRE(ctx.remotely());
}

TEST_CASE("no initial tasklet", "[testing][context]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr};

    REQUIRE(ctx.get_tasklet() == nullptr);
}

TEST_CASE("with initial tasklet", "[testing][context]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto t0 = create_tasklet_tracker("pool", "t0");
    testing_request_context ctx{resources, t0};

    REQUIRE(ctx.get_tasklet() == t0);
}

TEST_CASE("push/pop tasklet", "[testing][context]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto t0 = create_tasklet_tracker("pool", "t0");
    auto t1 = create_tasklet_tracker("pool", "t1");
    testing_request_context ctx{resources, nullptr};

    REQUIRE(ctx.get_tasklet() == nullptr);
    ctx.push_tasklet(t0);
    REQUIRE(ctx.get_tasklet() == t0);
    ctx.push_tasklet(t1);
    REQUIRE(ctx.get_tasklet() == t1);
    ctx.pop_tasklet();
    REQUIRE(ctx.get_tasklet() == t0);
    ctx.pop_tasklet();
    REQUIRE(ctx.get_tasklet() == nullptr);
}

TEST_CASE("set/get proxy name", "[testing][context]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr};

    ctx.proxy_name("the name");
    REQUIRE(ctx.proxy_name() == "the name");
}

TEST_CASE("get unset proxy name", "[testing][context]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr};

    REQUIRE_THROWS(ctx.proxy_name());
}

TEST_CASE("domain_name()", "[testing][context]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr};

    REQUIRE(ctx.domain_name() == "testing");
}

TEST_CASE("local_clone()", "[testing][context]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto t0 = create_tasklet_tracker("pool", "t0");
    testing_request_context ctx{resources, t0, true};
    ctx.proxy_name("the name");

    auto ctx1_intf{ctx.local_clone()};

    REQUIRE(dynamic_cast<testing_request_context*>(&*ctx1_intf) != nullptr);
    auto& ctx1{static_cast<testing_request_context&>(*ctx1_intf)};
    // Same as the original
    REQUIRE(&ctx1.get_resources() == &ctx.get_resources());
    REQUIRE(
        &ctx1.get_resources().memory_cache()
        == &ctx.get_resources().memory_cache());
    REQUIRE(ctx1.domain_name() == ctx.domain_name());
    // Differences with the original
    REQUIRE(ctx1.get_tasklet() == nullptr);
    REQUIRE_THROWS(ctx1.proxy_name());
    REQUIRE(!ctx1.remotely());
}
