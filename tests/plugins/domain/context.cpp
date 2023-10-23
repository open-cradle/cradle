#include <catch2/catch.hpp>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>

#include "../../support/inner_service.h"

using namespace cradle;

TEST_CASE("get_resources()", "[testing][context]")
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, nullptr, ""};

    REQUIRE(&ctx.get_resources() == &*resources);
    REQUIRE(&ctx.get_resources().memory_cache() == &resources->memory_cache());
}

TEST_CASE("remotely(), default", "[testing][context]")
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, nullptr, ""};

    REQUIRE(!ctx.remotely());
}

TEST_CASE("remotely(), set", "[testing][context]")
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, nullptr, "some_proxy"};

    REQUIRE(ctx.remotely());
}

TEST_CASE("no initial tasklet", "[testing][context]")
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, nullptr, ""};

    REQUIRE(ctx.get_tasklet() == nullptr);
}

TEST_CASE("with initial tasklet", "[testing][context]")
{
    auto resources{make_inner_test_resources()};
    auto t0 = create_tasklet_tracker("pool", "t0");
    testing_request_context ctx{*resources, t0, ""};

    REQUIRE(ctx.get_tasklet() == t0);
}

TEST_CASE("push/pop tasklet", "[testing][context]")
{
    auto resources{make_inner_test_resources()};
    auto t0 = create_tasklet_tracker("pool", "t0");
    auto t1 = create_tasklet_tracker("pool", "t1");
    testing_request_context ctx{*resources, nullptr, ""};

    REQUIRE(ctx.get_tasklet() == nullptr);
    ctx.push_tasklet(*t0);
    REQUIRE(ctx.get_tasklet() == t0);
    ctx.push_tasklet(*t1);
    REQUIRE(ctx.get_tasklet() == t1);
    ctx.pop_tasklet();
    REQUIRE(ctx.get_tasklet() == t0);
    ctx.pop_tasklet();
    REQUIRE(ctx.get_tasklet() == nullptr);
}

TEST_CASE("get proxy name", "[testing][context]")
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, nullptr, "the name"};

    REQUIRE(ctx.proxy_name() == "the name");
}

TEST_CASE("domain_name()", "[testing][context]")
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, nullptr, ""};

    REQUIRE(ctx.domain_name() == "testing");
}
