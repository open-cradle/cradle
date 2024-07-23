#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../inner-dll/v1/adder_v1.h"
#include "../../inner-dll/v1/adder_v1_impl.h"
#include "../../support/cancel_async.h"
#include "../../support/inner_service.h"
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/test_dlls_dir.h>

namespace cradle {

namespace {

char const tag[] = "[inner][resolve][contained]";

const containment_data v1_containment{
    request_uuid{adder_v1p_uuid}, get_test_dlls_dir(), "test_inner_dll_v1"};

const containment_data coro_v1_containment{
    request_uuid{coro_v1p_uuid}, get_test_dlls_dir(), "test_inner_dll_v1"};

auto
make_v1_resources(std::string const& proxy_name)
{
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    // The v1 functions are linked into the test runner for local evaluation.
    if (!proxy_name.empty())
    {
        auto& proxy{resources->get_proxy(proxy_name)};
        proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");
    }
    return resources;
}

int
get_num_contained_calls(
    remote_context_intf& ctx, std::string const& proxy_name)
{
    auto& resources{ctx.get_resources()};
    if (proxy_name.empty())
    {
        return resources.get_num_contained_calls();
    }
    else
    {
        auto const& proxy{resources.get_proxy(proxy_name)};
        return proxy.get_num_contained_calls();
    }
}

void
test_uncontained(
    std::string const& proxy_name,
    testing_request_context& ctx,
    auto const& req)
{
    auto const prev_calls{get_num_contained_calls(ctx, proxy_name)};
    REQUIRE(cppcoro::sync_wait(resolve_request(ctx, req)) == 7 + 2);
    REQUIRE(get_num_contained_calls(ctx, proxy_name) == prev_calls);
}

void
test_uncontained_all(std::string const& proxy_name)
{
    auto resources{make_v1_resources(proxy_name)};
    testing_request_context ctx{*resources, proxy_name};

    // No containment info passed to the request factory function
    test_uncontained(proxy_name, ctx, rq_test_adder_v1p_impl(7, 2));
    test_uncontained(proxy_name, ctx, rq_test_adder_v1n_impl(7, 2));
    if (!proxy_name.empty())
    {
        test_uncontained(proxy_name, ctx, rq_test_adder_v1p(7, 2));
        test_uncontained(proxy_name, ctx, rq_test_adder_v1n(7, 2));
    }

    // nullptr containment info passed to the request factory function
    test_uncontained(proxy_name, ctx, rq_test_adder_v1p_impl(nullptr, 7, 2));
    test_uncontained(proxy_name, ctx, rq_test_adder_v1n_impl(nullptr, 7, 2));
    if (!proxy_name.empty())
    {
        test_uncontained(proxy_name, ctx, rq_test_adder_v1p(nullptr, 7, 2));
        test_uncontained(proxy_name, ctx, rq_test_adder_v1n(nullptr, 7, 2));
    }
}

} // namespace

TEST_CASE("resolve uncontained - local", tag)
{
    test_uncontained_all("");
}

TEST_CASE("resolve uncontained - loopback", tag)
{
    test_uncontained_all("loopback");
}

TEST_CASE("resolve uncontained - rpclib", tag)
{
    test_uncontained_all("rpclib");
}

namespace {

void
test_contained(
    std::string const& proxy_name, Context auto& ctx, auto const& req)
{
    auto const prev_calls{get_num_contained_calls(ctx, proxy_name)};
    REQUIRE(cppcoro::sync_wait(resolve_request(ctx, req)) == 7 + 2);
    REQUIRE(get_num_contained_calls(ctx, proxy_name) == prev_calls + 1);
}

void
test_contained_all(Context auto& ctx, std::string const& proxy_name)
{
    auto const* cp = &v1_containment;
    test_contained(proxy_name, ctx, rq_test_adder_v1p_impl(cp, 7, 2));
    test_contained(proxy_name, ctx, rq_test_adder_v1n_impl(cp, 7, 2));
    if (!proxy_name.empty())
    {
        test_contained(proxy_name, ctx, rq_test_adder_v1p(cp, 7, 2));
        test_contained(proxy_name, ctx, rq_test_adder_v1n(cp, 7, 2));
    }
}

void
test_contained_all_sync(std::string const& proxy_name)
{
    auto resources{make_v1_resources(proxy_name)};
    testing_request_context ctx{*resources, proxy_name};
    test_contained_all(ctx, proxy_name);
}

void
test_contained_all_async(std::string const& proxy_name)
{
    auto resources{make_v1_resources(proxy_name)};
    atst_context ctx{*resources, proxy_name};
    test_contained_all(ctx, proxy_name);
}

} // namespace

TEST_CASE("resolve contained - local, sync", tag)
{
    test_contained_all_sync("");
}

TEST_CASE("resolve contained - loopback, sync", tag)
{
    test_contained_all_sync("loopback");
}

TEST_CASE("resolve contained - rpclib, sync", tag)
{
    test_contained_all_sync("rpclib");
}

TEST_CASE("resolve contained - local, async", tag)
{
    test_contained_all_async("");
}

TEST_CASE("resolve contained - loopback, async", tag)
{
    test_contained_all_async("loopback");
}

TEST_CASE("resolve contained - rpclib, async", tag)
{
    test_contained_all_async("rpclib");
}

namespace {

void
test_subreq(
    std::string const& proxy_name,
    testing_request_context& ctx,
    auto const& req)
{
    auto const prev_calls{get_num_contained_calls(ctx, proxy_name)};
    REQUIRE(cppcoro::sync_wait(resolve_request(ctx, req)) == 1 + 2 + 4);
    REQUIRE(get_num_contained_calls(ctx, proxy_name) == prev_calls + 2);
}

void
test_subreq_all(std::string const& proxy_name)
{
    auto resources{make_v1_resources(proxy_name)};
    testing_request_context ctx{*resources, proxy_name};

    auto const* cp = &v1_containment;
    auto subreq_v1n_impl{rq_test_adder_v1n_impl(cp, 1, 2)};
    auto subreq_v1p_impl{rq_test_adder_v1p_impl(cp, 1, 2)};
    test_subreq(
        proxy_name, ctx, rq_test_adder_v1n_impl(cp, subreq_v1n_impl, 4));
    test_subreq(
        proxy_name, ctx, rq_test_adder_v1n_impl(cp, subreq_v1p_impl, 4));
    if (!proxy_name.empty())
    {
        auto subreq_v1n{rq_test_adder_v1n(cp, 1, 2)};
        auto subreq_v1p{rq_test_adder_v1p(cp, 1, 2)};
        test_subreq(proxy_name, ctx, rq_test_adder_v1n(cp, subreq_v1n, 4));
        test_subreq(proxy_name, ctx, rq_test_adder_v1n(cp, subreq_v1p, 4));
    }
}

} // namespace

TEST_CASE("resolve contained with subreq - local", tag)
{
    test_subreq_all("");
}

TEST_CASE("resolve contained with subreq - loopback", tag)
{
    test_subreq_all("loopback");
}

TEST_CASE("resolve contained with subreq - rpclib", tag)
{
    test_subreq_all("rpclib");
}

namespace {

void
test_crash(std::string const& proxy_name)
{
    auto const* cp = &v1_containment;
    auto req{rq_test_adder_v1p_impl(cp, 7, adder_v1_b_crash)};
    auto resources{make_v1_resources(proxy_name)};
    testing_request_context ctx{*resources, proxy_name};
    auto const prev_calls{get_num_contained_calls(ctx, proxy_name)};
    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req)),
        Catch::Contains("timeout"));
    REQUIRE(get_num_contained_calls(ctx, proxy_name) == prev_calls + 1);

    // Check that the server is still responsive.
    auto req1{rq_test_adder_v1p_impl(3, 4)};
    REQUIRE(cppcoro::sync_wait(resolve_request(ctx, req1)) == 3 + 4);
}

} // namespace

// This test case would crash the test runner.
// TEST_CASE("resolve uncontained crash - local", tag)

// This test case would crash the test runner.
// TEST_CASE("resolve uncontained crash - loopback", tag)

// This test case would crash the rpclib server and hang the test runner.
// TEST_CASE("resolve uncontained crash - rpclib", tag)

TEST_CASE("resolve contained crash - local", tag)
{
    test_crash("");
}

TEST_CASE("resolve contained crash - loopback", tag)
{
    test_crash("loopback");
}

TEST_CASE("resolve contained crash - rpclib", tag)
{
    test_crash("rpclib");
}

namespace {

void
test_cancel_async_remote(std::string const& proxy_name)
{
    constexpr int loops = 10;
    int delay0 = 5;
    int delay1 = 60;
    auto const* cp = &coro_v1_containment;
    auto req{rq_test_coro_v1n(
        rq_test_coro_v1p(loops, delay0), rq_test_coro_v1p(cp, loops, delay1))};
    auto resources{make_v1_resources(proxy_name)};

    test_cancel_async(*resources, proxy_name, req);
}

} // namespace

TEST_CASE("cancel contained request locally", tag)
{
    std::string proxy_name{""};
    auto const* cp = &coro_v1_containment;
    auto req{rq_test_coro_v1n_impl(
        rq_test_coro_v1p_impl(cp, 100, 7), rq_test_coro_v1p_impl(100, 8))};
    auto resources{make_v1_resources(proxy_name)};

    test_cancel_async(*resources, proxy_name, req);
}

TEST_CASE("cancel contained request on loopback", tag)
{
    test_cancel_async_remote("loopback");
}

TEST_CASE("cancel contained request on rpclib", tag)
{
    test_cancel_async_remote("rpclib");
}

namespace {

template<caching_level_type Level>
void
test_contained_caching()
{
    std::string proxy_name{""};
    auto resources{make_v1_resources(proxy_name)};
    atst_context ctx{*resources, proxy_name};

    auto const* cp = &v1_containment;
    auto req{rq_test_coro_v1n_impl<Level>(
        cp, rq_test_coro_v1n_impl<Level>(cp, 1, 4), 3)};
    auto const calls0{get_num_contained_calls(ctx, proxy_name)};

    REQUIRE(cppcoro::sync_wait(resolve_request(ctx, req)) == 1 + 4 + 3);
    REQUIRE(get_num_contained_calls(ctx, proxy_name) == calls0 + 2);

    REQUIRE(cppcoro::sync_wait(resolve_request(ctx, req)) == 1 + 4 + 3);
    int delta1 = is_uncached(Level) ? 2 : 0;
    REQUIRE(get_num_contained_calls(ctx, proxy_name) == calls0 + 2 + delta1);
}

} // namespace

TEST_CASE("resolve contained local - uncached", tag)
{
    test_contained_caching<caching_level_type::none>();
}

TEST_CASE("resolve contained local - memory cached", tag)
{
    test_contained_caching<caching_level_type::memory>();
}

TEST_CASE("resolve contained - submit_async failure", tag)
{
    std::string proxy_name{""};
    constexpr int loops = 10;
    int delay = 5;
    auto const* cp = &coro_v1_containment;
    auto req{rq_test_coro_v1n_impl(cp, loops, delay)};
    // TODO cf. resolve_request_one_try(): hangup if another thread calls
    // ctx.get_num_subs(), and using illegal proxy request:
    // auto req{rq_test_coro_v1p(cp, loops, delay)};
    auto resources{make_v1_resources(proxy_name)};
    atst_context ctx{*resources, proxy_name};

    // Force the submit_async RPC call to the contained process to fail
    ctx.fail_submit_async();

    CHECK_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req)),
        Catch::Contains("submit_async forced failure"));
}

namespace {

void
cancelling_func(atst_context& ctx)
{
    auto logger{ensure_logger("cancelling_func")};
    logger->info("start and wait for subprocess");
    // Wait until the contained process is active; indicated by creq_controller
    // setting the delegate on ctx.
    for (;;)
    {
        if (ctx.get_delegate())
        {
            break;
        }
        auto status = ctx.get_status();
        if (is_final(status))
        {
            logger->error(
                "unexpected final status {} before delegate",
                to_string(status));
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    logger->info("subprocess active (status {})", to_string(ctx.get_status()));
    logger->info("sleep 100ms");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger->info("call request_cancellation");
    ctx.request_cancellation();
    logger->info("returned from request_cancellation");
}

} // namespace

// Covering creq_context::set_remote_id() propagating cancel request
// t0        : resolve_request() below
// t1        : contained process started and accessible
// t1 + 100ms: cancelling_func() calls request_cancellation
// t1 + 200ms: creq_context::set_remote_id() cancels proxy
//             contained process cancelling its operation
//             ... and returning async_status::CANCELLED
//             resolve_request() aborted
TEST_CASE("resolve contained - cancel after process active", tag)
{
    std::string proxy_name{""};
    constexpr int loops = 10;
    int delay = 5;
    auto const* cp = &coro_v1_containment;
    auto req{rq_test_coro_v1n_impl(cp, loops, delay)};
    auto resources{make_v1_resources(proxy_name)};
    atst_context ctx{*resources, proxy_name};

    // The remote id will be returned after this delay
    ctx.set_submit_async_delay(200);

    std::jthread cancelling_thread(cancelling_func, std::ref(ctx));

    CHECK_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req)),
        Catch::Matches("remote async \\d+ cancelled"));

    cancelling_thread.join();
}

namespace {

void
immediately_cancelling_func(atst_context& ctx)
{
    auto logger{ensure_logger("cancelling_func")};
    logger->info("call request_cancellation");
    ctx.request_cancellation();
    logger->info("returned from request_cancellation");
}

} // namespace

// Covering creq_context::throw_if_cancelled() throwing
TEST_CASE("resolve contained - cancel immediately", tag)
{
    std::string proxy_name{""};
    constexpr int loops = 10;
    int delay = 5;
    auto const* cp = &coro_v1_containment;
    auto req{rq_test_coro_v1n_impl(cp, loops, delay)};
    auto resources{make_v1_resources(proxy_name)};
    atst_context ctx{*resources, proxy_name};

    // TODO this would hang
    // ctx.request_cancellation();

    ctx.set_submit_async_delay(100);

    std::jthread cancelling_thread(immediately_cancelling_func, std::ref(ctx));

    CHECK_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req)),
        "creq_context cancelled");

    cancelling_thread.join();
}

} // namespace cradle
