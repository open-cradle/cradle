#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../support/common.h"
#include "../../support/inner_service.h"
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/demo_class.h>
#include <cradle/plugins/domain/testing/demo_class_requests.h>

using namespace cradle;

namespace {

static char const tag[] = "[demo_class]";

// Tests resolving the two requests related to demo_class
template<caching_level_type caching_level>
static void
test_demo_class(std::string const& proxy_name)
{
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option{})};
    testing_request_context ctx{*resources, proxy_name};

    auto req0{rq_make_demo_class<caching_level>(3, 9)};
    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req0));
    REQUIRE(res0.get_x() == 3);
    REQUIRE(res0.get_y() == 9);

    auto req1{rq_copy_demo_class<caching_level>(demo_class(5, 6))};
    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req1));
    REQUIRE(res1.get_x() == 5);
    REQUIRE(res1.get_y() == 6);
}

} // namespace

TEST_CASE("demo_class - none, local", tag)
{
    test_demo_class<caching_level_type::none>("");
}

TEST_CASE("demo_class - memory, local", tag)
{
    test_demo_class<caching_level_type::memory>("");
}

TEST_CASE("demo_class - full, local", tag)
{
    test_demo_class<caching_level_type::full>("");
}

TEST_CASE("demo_class - none, loopback", tag)
{
    test_demo_class<caching_level_type::none>("loopback");
}

TEST_CASE("demo_class - memory, loopback", tag)
{
    test_demo_class<caching_level_type::memory>("loopback");
}

TEST_CASE("demo_class - full, loopback", tag)
{
    test_demo_class<caching_level_type::full>("loopback");
}

TEST_CASE("demo_class - none, rpclib", tag)
{
    test_demo_class<caching_level_type::none>("rpclib");
}

TEST_CASE("demo_class - memory, rpclib", tag)
{
    test_demo_class<caching_level_type::memory>("rpclib");
}

TEST_CASE("demo_class - full, rpclib", tag)
{
    test_demo_class<caching_level_type::full>("rpclib");
}
