#include <catch2/catch.hpp>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_impl.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>

#include "../../support/inner_service.h"

using namespace cradle;

static char const tag[] = "[testing][context]";

TEST_CASE("get_resources()", tag)
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, ""};

    REQUIRE(&ctx.get_resources() == &*resources);
    REQUIRE(&ctx.get_resources().memory_cache() == &resources->memory_cache());
}

TEST_CASE("remotely(), default", tag)
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, ""};

    REQUIRE(!ctx.remotely());
}

TEST_CASE("remotely(), set", tag)
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, "some_proxy"};

    REQUIRE(ctx.remotely());
}

TEST_CASE("no initial tasklet", tag)
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, ""};

    REQUIRE(ctx.get_tasklet() == nullptr);
}

TEST_CASE("with initial tasklet", tag)
{
    auto resources{make_inner_test_resources()};
    auto& admin{resources->the_tasklet_admin()};
    admin.set_capturing_enabled(true);
    std::string pool_name{"pool"};
    std::string title{"title"};
    testing_request_context ctx{
        *resources, "", root_tasklet_spec{pool_name, title}};

    auto* tasklet = ctx.get_tasklet();
    REQUIRE(tasklet != nullptr);
    auto* impl = dynamic_cast<tasklet_impl*>(tasklet);
    REQUIRE(impl != nullptr);
    REQUIRE(impl->pool_name() == pool_name);
    REQUIRE(impl->title() == title);
}

TEST_CASE("push/pop tasklet", tag)
{
    auto resources{make_inner_test_resources()};
    auto& admin{resources->the_tasklet_admin()};
    auto t0 = create_tasklet_tracker(admin, "pool", "t0");
    auto t1 = create_tasklet_tracker(admin, "pool", "t1");
    testing_request_context ctx{*resources, ""};

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

TEST_CASE("domain_name()", tag)
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, ""};

    REQUIRE(ctx.domain_name() == "testing");
}

TEST_CASE("missing track_blob_file_writers() call", tag)
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, ""};

    ctx.make_data_owner(10, true);
    REQUIRE_THROWS_WITH(
        ctx.on_value_complete(),
        "on_value_complete() without preceding track_blob_file_writers()");
}
