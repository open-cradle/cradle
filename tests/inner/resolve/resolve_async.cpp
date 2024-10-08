#include <functional>
#include <thread>

#include <catch2/catch.hpp>
#include <cppcoro/schedule_on.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <fmt/format.h>

#include "../../support/cancel_async.h"
#include "../../support/inner_service.h"
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/domain_factory.h>
#include <cradle/plugins/domain/testing/requests.h>

namespace cradle {

namespace {

static char const tag[] = "[inner][resolve][async]";

request_uuid
make_test_uuid(int ext)
{
    return request_uuid{fmt::format("{}-{:04d}", tag, ext)};
}

template<typename Ctx, typename Req, typename Constraints>
cppcoro::task<void>
test_resolve_async_coro(
    Ctx& main_ctx,
    Req const& req,
    Constraints constraints,
    bool requests_are_normalized,
    int loops,
    int delay0,
    int delay1)
{
    auto res = co_await resolve_request(main_ctx, req, constraints);
    auto& ctx = main_ctx.get_async_root();

    REQUIRE(res == (loops + delay0) + (loops + delay1));
    REQUIRE(ctx.is_req());
    REQUIRE(co_await ctx.get_status_coro() == async_status::FINISHED);
    REQUIRE(ctx.get_num_subs() == 2);
    auto& ctx0 = ctx.get_sub(0);
    REQUIRE(ctx.is_req());
    REQUIRE(ctx0.get_num_subs() == 2);
    REQUIRE(co_await ctx0.get_status_coro() == async_status::FINISHED);
    auto& ctx1 = ctx.get_sub(1);
    REQUIRE(ctx.is_req());
    REQUIRE(ctx1.get_num_subs() == 2);
    REQUIRE(co_await ctx1.get_status_coro() == async_status::FINISHED);
    if (requests_are_normalized)
    {
        auto& ctx00 = ctx0.get_sub(0);
        REQUIRE(ctx00.is_req());
        REQUIRE(ctx00.get_num_subs() == 1);
        REQUIRE(co_await ctx00.get_status_coro() == async_status::FINISHED);
        auto& ctx000 = ctx00.get_sub(0);
        REQUIRE(!ctx000.is_req());
        REQUIRE(co_await ctx000.get_status_coro() == async_status::FINISHED);
        auto& ctx01 = ctx0.get_sub(1);
        REQUIRE(ctx01.is_req());
        REQUIRE(ctx01.get_num_subs() == 1);
        REQUIRE(co_await ctx01.get_status_coro() == async_status::FINISHED);
        auto& ctx010 = ctx01.get_sub(0);
        REQUIRE(!ctx010.is_req());
        REQUIRE(co_await ctx010.get_status_coro() == async_status::FINISHED);
        auto& ctx10 = ctx1.get_sub(0);
        REQUIRE(ctx10.is_req());
        REQUIRE(ctx10.get_num_subs() == 1);
        REQUIRE(co_await ctx10.get_status_coro() == async_status::FINISHED);
        auto& ctx100 = ctx10.get_sub(0);
        REQUIRE(!ctx100.is_req());
        REQUIRE(co_await ctx100.get_status_coro() == async_status::FINISHED);
        auto& ctx11 = ctx1.get_sub(1);
        REQUIRE(ctx11.is_req());
        REQUIRE(ctx11.get_num_subs() == 1);
        REQUIRE(co_await ctx11.get_status_coro() == async_status::FINISHED);
        auto& ctx110 = ctx11.get_sub(0);
        REQUIRE(!ctx110.is_req());
        REQUIRE(co_await ctx110.get_status_coro() == async_status::FINISHED);
    }
    else
    {
        auto& ctx00 = ctx0.get_sub(0);
        REQUIRE(!ctx00.is_req());
        REQUIRE(co_await ctx00.get_status_coro() == async_status::FINISHED);
        auto& ctx01 = ctx0.get_sub(1);
        REQUIRE(!ctx01.is_req());
        REQUIRE(co_await ctx01.get_status_coro() == async_status::FINISHED);
        auto& ctx10 = ctx1.get_sub(0);
        REQUIRE(!ctx10.is_req());
        REQUIRE(co_await ctx10.get_status_coro() == async_status::FINISHED);
        auto& ctx11 = ctx1.get_sub(1);
        REQUIRE(!ctx11.is_req());
        REQUIRE(co_await ctx11.get_status_coro() == async_status::FINISHED);
    }
}

template<typename Ctx, typename Req, typename Constraints>
void
test_resolve_async(
    Ctx& ctx,
    Req const& req,
    Constraints constraints,
    bool requests_are_normalized,
    int loops,
    int delay0,
    int delay1)
{
    cppcoro::sync_wait(test_resolve_async_coro(
        ctx,
        req,
        constraints,
        requests_are_normalized,
        loops,
        delay0,
        delay1));
}

void
test_resolve_async_across_rpc(
    inner_resources& resources, std::string const& proxy_name)
{
    constexpr int loops = 3;
    constexpr auto level = caching_level_type::memory;
    int delay0 = 5;
    int delay1 = 60;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    ResolutionConstraintsRemoteAsync constraints;
    atst_context ctx{resources, proxy_name};

    // TODO clear the memory cache on the remote and check the duration
    test_resolve_async(ctx, req, constraints, true, loops, delay0, delay1);

    // TODO check the duration which should be fast because the result now
    // comes from the memory cache on the remote
    test_resolve_async(ctx, req, constraints, true, loops, delay0, delay1);
}

} // namespace

TEST_CASE("resolve async locally - raw args, coro", tag)
{
    constexpr int loops = 3;
    int delay0 = 5;
    int delay1 = 6;
    using Props = request_props<
        caching_level_type::none,
        request_function_t::coro,
        false>;
    Props props0{make_test_uuid(100)};
    Props props1{make_test_uuid(101)};
    Props props2{make_test_uuid(102)};
    // rq_cancellable_coro would call normalize_arg() on its arguments and
    // we don't want that in this test.
    auto req{rq_function(
        props0,
        cancellable_coro,
        rq_function(props1, cancellable_coro, loops, delay0),
        rq_function(props2, cancellable_coro, loops, delay1))};
    auto resources{make_inner_test_resources()};
    atst_context ctx{*resources};

    ResolutionConstraintsLocalAsyncRoot constraints;
    test_resolve_async(ctx, req, constraints, false, loops, delay0, delay1);
}

TEST_CASE("resolve async locally - raw args, non-coro", tag)
{
    constexpr int loops = 3;
    int delay0 = 5;
    int delay1 = 6;
    using Props = request_props<
        caching_level_type::none,
        request_function_t::plain,
        false>;
    Props props0{make_test_uuid(100)};
    Props props1{make_test_uuid(101)};
    Props props2{make_test_uuid(102)};
    auto req{rq_function(
        props0,
        non_cancellable_func,
        rq_function(props1, non_cancellable_func, loops, delay0),
        rq_function(props2, non_cancellable_func, loops, delay1))};
    auto resources{make_inner_test_resources()};
    atst_context ctx{*resources};

    ResolutionConstraintsLocalAsyncRoot constraints;
    test_resolve_async(ctx, req, constraints, false, loops, delay0, delay1);
}

TEST_CASE("resolve async locally - normalized args", tag)
{
    constexpr int loops = 3;
    int delay0 = 5;
    int delay1 = 6;
    constexpr auto level{caching_level_type::none};
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    auto resources{make_inner_test_resources()};
    atst_context ctx{*resources};

    ResolutionConstraintsLocalAsyncRoot constraints;
    test_resolve_async(ctx, req, constraints, true, loops, delay0, delay1);
}

TEST_CASE("resolve async on loopback", tag)
{
    std::string proxy_name{"loopback"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_resolve_async_across_rpc(*resources, proxy_name);
}

TEST_CASE("resolve async on rpclib", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_resolve_async_across_rpc(*resources, proxy_name);
}

TEST_CASE("resolve async with value_request locally", tag)
{
    constexpr int loops = 3;
    int delay0 = 5;
    int val1 = 6;
    constexpr auto level{caching_level_type::full};
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0), rq_value(val1))};
    auto resources{make_inner_test_resources()};
    atst_context ctx{*resources};

    ResolutionConstraintsLocalAsyncRoot constraints;
    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res0 == 14);

    resources->reset_memory_cache();
    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res1 == 14);
}

namespace {

template<typename Ctx, typename Req>
cppcoro::task<void>
test_error_async_coro(Ctx& ctx, Req const& req, auto const& matcher)
{
    REQUIRE_THROWS_MATCHES(
        co_await resolve_request(ctx, req), async_error, matcher);
    REQUIRE(co_await ctx.get_status_coro() == async_status::FAILED);
}

template<typename Ctx, typename Req>
void
test_error_async(Ctx& ctx, Req const& req)
{
    cppcoro::sync_wait(test_error_async_coro(
        ctx, req, Catch::Matchers::Message("cancellable_coro() failed")));
}

template<typename Ctx, typename Req>
void
test_error_async_plain(Ctx& ctx, Req const& req)
{
    cppcoro::sync_wait(test_error_async_coro(
        ctx, req, Catch::Matchers::Message("non_cancellable_func() failed")));
}

void
test_error_async_across_rpc(
    inner_resources& resources, std::string const& proxy_name)
{
    constexpr int loops = 2;
    constexpr auto level = caching_level_type::memory;
    int delay0 = 11;
    int delay1 = 24;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(-1, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    atst_context ctx{resources, proxy_name};

    test_error_async(ctx, req);
}

} // namespace

TEST_CASE("error async request locally - coro", tag)
{
    constexpr auto level = caching_level_type::none;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(-1, 11),
        rq_cancellable_coro<level>(2, 24))};
    auto resources{make_inner_test_resources()};
    atst_context ctx{*resources};

    test_error_async(ctx, req);
}

TEST_CASE("error async request locally - non-coro", tag)
{
    constexpr auto level = caching_level_type::none;
    auto req{rq_non_cancellable_func<level>(
        rq_non_cancellable_func<level>(-1, 11),
        rq_non_cancellable_func<level>(2, 24))};
    auto resources{make_inner_test_resources()};
    atst_context ctx{*resources};

    test_error_async_plain(ctx, req);
}

TEST_CASE("error async request on loopback", tag)
{
    std::string proxy_name{"loopback"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_error_async_across_rpc(*resources, proxy_name);
}

TEST_CASE("error async request on rpclib", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_error_async_across_rpc(*resources, proxy_name);
}

TEST_CASE("cancel async request locally", tag)
{
    std::string proxy_name{""};
    constexpr auto level{caching_level_type::none};
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(100, 7),
        rq_cancellable_coro<level>(100, 8))};
    auto resources{make_inner_test_resources()};

    test_cancel_async(*resources, proxy_name, req);
}

namespace {

void
test_cancel_async_across_rpc(std::string const& proxy_name)
{
    constexpr int loops = 10;
    constexpr auto level = caching_level_type::memory;
    int delay0 = 5;
    int delay1 = 60;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_cancel_async(*resources, proxy_name, req);
}

} // namespace

TEST_CASE("cancel async request on loopback", tag)
{
    test_cancel_async_across_rpc("loopback");
}

TEST_CASE("cancel async request on rpclib", tag)
{
    test_cancel_async_across_rpc("rpclib");
}

namespace {

// Attempts to retrieve information from the remote related to the given
// context. The get_num_subs() call should throw an exception because
// submit_async was forced to fail, so no remote id will ever be available.
// Runs on separate thread. Any exception thrown here, including the one thrown
// from a failing REQUIRE*(), won't be caught by Catch2, and lead to a
// terminate. Moreover, Catch2 is not thread-safe. So instead let the main
// thread do the check.
void
get_subs_control_func(async_context_intf& ctx, std::string& what)
{
    try
    {
        ctx.get_num_subs();
    }
    catch (std::exception const& e)
    {
        what = e.what();
    }
}

void
test_failing_get_num_subs(
    inner_resources& resources, std::string const& proxy_name)
{
    constexpr auto level = caching_level_type::memory;
    auto req{rq_cancellable_coro<level>(2, 3)};
    atst_context ctx{resources, proxy_name};

    // Causes submit_async to fail on the remote
    ctx.fail_submit_async();

    // Run get_num_subs on a separate thread, independent from the main one
    // which will call resolve_request().
    std::string thread_what;
    std::jthread control_thread(
        get_subs_control_func, std::ref(ctx), std::ref(thread_what));

    CHECK_THROWS(cppcoro::sync_wait(resolve_request(ctx, req)));

    control_thread.join();

    CHECK(thread_what.find("submit_async forced failure") != thread_what.npos);
}

} // namespace

TEST_CASE("get_num_subs failure on loopback", tag)
{
    std::string proxy_name{"loopback"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_failing_get_num_subs(*resources, proxy_name);
}

TEST_CASE("get_num_subs failure on rpclib", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_failing_get_num_subs(*resources, proxy_name);
}

namespace {

// The actual CHECKs are on the main thread due to Catch2's assertion macros
// not being thread-safe.
void
delayed_get_subs_control_func(
    async_context_intf& ctx,
    async_status& initial_status,
    std::size_t& num_subs)
{
    // Check that resolve_async on the remote is in the startup delay
    initial_status = cppcoro::sync_wait(ctx.get_status_coro());
    // CHECK(initial_status == async_status::CREATED;

    // get_num_subs should block until the information is available on the
    // remote. Status should be SUBS_RUNNING now or real soon, but "real soon"
    // implies we cannot check it.
    num_subs = ctx.get_num_subs();
    // CHECK(num_subs == 2);
}

void
test_delayed_get_num_subs(
    inner_resources& resources, std::string const& proxy_name)
{
    constexpr auto level = caching_level_type::memory;
    auto req{rq_cancellable_coro<level>(2, 3)};
    atst_context ctx{resources, proxy_name};

    // Forces resolve_async() on the remote to have a startup delay
    ctx.set_resolve_async_delay(500);

    // Run get_num_subs on a separate thread, independent from the main one
    // which will call resolve_request().
    async_status initial_status{};
    std::size_t num_subs{};
    std::jthread control_thread(
        delayed_get_subs_control_func,
        std::ref(ctx),
        std::ref(initial_status),
        std::ref(num_subs));

    CHECK(cppcoro::sync_wait(resolve_request(ctx, req)) == 5);

    control_thread.join();

    CHECK(initial_status == async_status::CREATED);
    CHECK(num_subs == 2);
}

} // namespace

// resolve_async() is forced to have a startup delay.
// The information that get_num_subs needs is available only after
// resolve_async has started, so get_num_subs needs to wait.
TEST_CASE("delayed get_num_subs on loopback", tag)
{
    std::string proxy_name{"loopback"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_delayed_get_num_subs(*resources, proxy_name);
}

TEST_CASE("delayed get_num_subs on rpclib", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_delayed_get_num_subs(*resources, proxy_name);
}

namespace {

// The actual CHECKs are on the main thread due to Catch2's assertion macros
// not being thread-safe.
void
delayed_set_result_control_func(
    async_context_intf& ctx,
    async_status& interim_status,
    async_status& final_status)
{
    // Let the calculation finish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check on the remote that the calculation has finished, but the result
    // was not yet stored (due to the 200ms set_result forced delay)
    interim_status = cppcoro::sync_wait(ctx.get_status_coro());
    // CHECK(interim_status == async_status::AWAITING_RESULT);

    // Let set_result() finish
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check that the calculation has now completely finished
    final_status = cppcoro::sync_wait(ctx.get_status_coro());
    // CHECK(final_status == async_status::FINISHED);
}

void
test_delayed_set_result(
    inner_resources& resources, std::string const& proxy_name)
{
    constexpr auto level = caching_level_type::memory;
    auto req{rq_cancellable_coro<level>(0, 0)};
    atst_context ctx{resources, proxy_name};

    // Forces set_result() on the remote to have a delay
    ctx.set_set_result_delay(200);

    // Create a separate control thread, independent from the main one which
    // will call resolve_request().
    async_status interim_status{};
    async_status final_status{};
    std::jthread control_thread(
        delayed_set_result_control_func,
        std::ref(ctx),
        std::ref(interim_status),
        std::ref(final_status));

    CHECK(cppcoro::sync_wait(resolve_request(ctx, req)) == 0);

    control_thread.join();

    CHECK(interim_status == async_status::AWAITING_RESULT);
    CHECK(final_status == async_status::FINISHED);
}

} // namespace

// set_result() is forced to have a 200ms delay going from AWAITING_RESULT to
// FINISHED
TEST_CASE("delayed set_result on loopback", tag)
{
    std::string proxy_name{"loopback"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_delayed_set_result(*resources, proxy_name);
}

TEST_CASE("delayed set_result on rpclib", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    test_delayed_set_result(*resources, proxy_name);
}

TEST_CASE("create rq_cancellable_coro with different caching levels", tag)
{
    REQUIRE_NOTHROW(rq_cancellable_coro<caching_level_type::none>(0, 0));
    REQUIRE_NOTHROW(rq_cancellable_coro<caching_level_type::memory>(0, 0));
    REQUIRE_NOTHROW(rq_cancellable_coro<caching_level_type::full>(0, 0));
}

TEST_CASE("create rq_cancellable_coro with different loop/delay values", tag)
{
    REQUIRE_NOTHROW(rq_cancellable_coro<caching_level_type::full>(0, 1));
    REQUIRE_NOTHROW(rq_cancellable_coro<caching_level_type::full>(1, 0));
}

TEST_CASE("create rq_cancellable_coro with different loop/delay types", tag)
{
    REQUIRE_NOTHROW(
        rq_cancellable_coro<caching_level_type::full, unsigned, int>(0, 0));
    REQUIRE_NOTHROW(
        rq_cancellable_coro<caching_level_type::full, int, unsigned>(0, 0));
}

TEST_CASE("resolve proxy async on rpclib", tag)
{
    std::string proxy_name{"rpclib"};
    constexpr int loops = 3;
    constexpr auto level = caching_level_type::memory;
    int delay0 = 5;
    int delay1 = 60;
    auto req{rq_cancellable_proxy<level>(
        rq_cancellable_proxy<level>(loops, delay0),
        rq_cancellable_proxy<level>(loops, delay1))};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    ResolutionConstraintsRemoteAsync constraints;
    atst_context ctx{*resources, proxy_name};

    test_resolve_async(ctx, req, constraints, true, loops, delay0, delay1);
}

} // namespace cradle
