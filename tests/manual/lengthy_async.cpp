#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../support/inner_service.h"
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/domain_factory.h>
#include <cradle/plugins/domain/testing/requests.h>

namespace cradle {

namespace {

#define TAG(n) "[manual][" #n "]"

} // namespace

TEST_CASE("lengthy async on rpclib", "[A]")
{
    std::string proxy_name{"rpclib"};
    constexpr int loops = 3;
    constexpr auto level = caching_level_type::memory;
    int delay0 = 5;
    int delay1 = 6000;
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    ResolutionConstraintsRemoteAsync constraints;
    atst_context ctx{*resources, proxy_name};
    ctx.make_introspective();

    auto res = cppcoro::sync_wait(resolve_request(ctx, req, constraints));

    REQUIRE(res == (loops + delay0) + (loops + delay1));
}

} // namespace cradle
