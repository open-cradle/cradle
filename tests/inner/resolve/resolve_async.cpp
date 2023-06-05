#include <functional>
#include <thread>

#include <catch2/catch.hpp>
#include <cppcoro/schedule_on.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <fmt/format.h>

#include "../../support/inner_service.h"
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/domain.h>
#include <cradle/plugins/domain/testing/requests.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][service][async]";

request_uuid
make_test_uuid(int ext)
{
    return request_uuid{fmt::format("{}-{:04d}", tag, ext)};
}

// TODO? have two inner_resources instances; one for test, one for loopback
void
setup_loopback_test(inner_resources& inner)
{
    init_test_inner_service(inner);
    inner.ensure_async_db();
    ensure_loopback_service(inner);
    register_and_initialize_testing_domain();
}

void
setup_rpclib_test(inner_resources& inner)
{
    init_test_inner_service(inner);
    ensure_rpclib_service();
}

template<typename Ctx, typename Req, typename Constraints>
cppcoro::task<void>
test_resolve_async_coro(
    Ctx& ctx,
    Req const& req,
    Constraints constraints,
    bool requests_are_normalized,
    int loops,
    int delay0,
    int delay1)
{
    auto res = co_await resolve_request(ctx, req, constraints);

    REQUIRE(res == (loops + delay0) + (loops + delay1));
    REQUIRE(ctx.is_req());
    REQUIRE(co_await ctx.get_status_coro() == async_status::FINISHED);
    REQUIRE(co_await ctx.get_num_subs() == 2);
    auto& ctx0 = ctx.get_sub(0);
    REQUIRE(ctx.is_req());
    REQUIRE(co_await ctx0.get_num_subs() == 2);
    REQUIRE(co_await ctx0.get_status_coro() == async_status::FINISHED);
    auto& ctx1 = ctx.get_sub(1);
    REQUIRE(ctx.is_req());
    REQUIRE(co_await ctx1.get_num_subs() == 2);
    REQUIRE(co_await ctx1.get_status_coro() == async_status::FINISHED);
    if (requests_are_normalized)
    {
        auto& ctx00 = ctx0.get_sub(0);
        REQUIRE(ctx00.is_req());
        REQUIRE(co_await ctx00.get_num_subs() == 1);
        REQUIRE(co_await ctx00.get_status_coro() == async_status::FINISHED);
        auto& ctx000 = ctx00.get_sub(0);
        REQUIRE(!ctx000.is_req());
        REQUIRE(co_await ctx000.get_status_coro() == async_status::FINISHED);
        auto& ctx01 = ctx0.get_sub(1);
        REQUIRE(ctx01.is_req());
        REQUIRE(co_await ctx01.get_num_subs() == 1);
        REQUIRE(co_await ctx01.get_status_coro() == async_status::FINISHED);
        auto& ctx010 = ctx01.get_sub(0);
        REQUIRE(!ctx010.is_req());
        REQUIRE(co_await ctx010.get_status_coro() == async_status::FINISHED);
        auto& ctx10 = ctx1.get_sub(0);
        REQUIRE(ctx10.is_req());
        REQUIRE(co_await ctx10.get_num_subs() == 1);
        REQUIRE(co_await ctx10.get_status_coro() == async_status::FINISHED);
        auto& ctx100 = ctx10.get_sub(0);
        REQUIRE(!ctx100.is_req());
        REQUIRE(co_await ctx100.get_status_coro() == async_status::FINISHED);
        auto& ctx11 = ctx1.get_sub(1);
        REQUIRE(ctx11.is_req());
        REQUIRE(co_await ctx11.get_num_subs() == 1);
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
    inner_resources& inner, std::string const& proxy_name)
{
    constexpr int loops = 3;
    constexpr auto level = caching_level_type::memory;
    int delay0 = 5;
    int delay1 = 60;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    auto tree_ctx{
        std::make_shared<proxy_atst_tree_context>(inner, proxy_name)};
    ResolutionConstraintsRemoteAsync constraints;

    // TODO clear the memory cache on the remote and check the duration
    auto ctx0{root_proxy_atst_context{tree_ctx, true}};
    test_resolve_async(ctx0, req, constraints, true, loops, delay0, delay1);

    // TODO check the duration which should be fast because the result now
    // comes from the memory cache on the remote
    auto ctx1{root_proxy_atst_context{tree_ctx, true}};
    test_resolve_async(ctx1, req, constraints, true, loops, delay0, delay1);
}

} // namespace

TEST_CASE("resolve async locally", tag)
{
    constexpr int loops = 3;
    using required_ctx_types = ctx_type_list<local_async_context_intf>;
    using Props = request_props<
        caching_level_type::none,
        true,
        false,
        required_ctx_types>;
    Props props0{make_test_uuid(100)};
    Props props1{make_test_uuid(101)};
    Props props2{make_test_uuid(102)};
    int delay0 = 5;
    int delay1 = 6;
    auto req{rq_function_erased(
        props0,
        cancellable_coro,
        rq_function_erased(props1, cancellable_coro, loops, delay0),
        rq_function_erased(props2, cancellable_coro, loops, delay1))};
    inner_resources inner;
    init_test_inner_service(inner);
    auto tree_ctx = std::make_shared<local_atst_tree_context>(inner);
    auto root_ctx{make_local_async_ctx_tree(tree_ctx, req)};

    ResolutionConstraintsLocalAsync constraints;
    test_resolve_async(
        *root_ctx, req, constraints, false, loops, delay0, delay1);
}

TEST_CASE("resolve async on loopback", tag)
{
    inner_resources inner;
    setup_loopback_test(inner);

    test_resolve_async_across_rpc(inner, "loopback");
}

TEST_CASE("resolve async on rpclib", tag)
{
    inner_resources inner;
    setup_rpclib_test(inner);

    test_resolve_async_across_rpc(inner, "rpclib");
}

namespace {

template<typename Ctx, typename Req>
cppcoro::task<void>
test_error_async_coro(Ctx& ctx, Req const& req)
{
    REQUIRE_THROWS_MATCHES(
        co_await resolve_request(ctx, req),
        async_error,
        Catch::Matchers::Message("cancellable_coro() failed"));
    REQUIRE(co_await ctx.get_status_coro() == async_status::ERROR);
}

template<typename Ctx, typename Req>
void
test_error_async(Ctx& ctx, Req const& req)
{
    cppcoro::sync_wait(test_error_async_coro(ctx, req));
}

void
test_error_async_across_rpc(
    inner_resources& inner, std::string const& proxy_name)
{
    constexpr int loops = 2;
    constexpr auto level = caching_level_type::memory;
    int delay0 = 11;
    int delay1 = 24;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(-1, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    auto tree_ctx{
        std::make_shared<proxy_atst_tree_context>(inner, proxy_name)};
    auto ctx{root_proxy_atst_context{tree_ctx, true}};

    test_error_async(ctx, req);
}

} // namespace

TEST_CASE("error async request locally", tag)
{
    using required_ctx_types = ctx_type_list<local_async_context_intf>;
    using Props = request_props<
        caching_level_type::none,
        true,
        false,
        required_ctx_types>;
    Props props0{make_test_uuid(300)};
    Props props1{make_test_uuid(301)};
    Props props2{make_test_uuid(302)};
    auto req{rq_function_erased(
        props0,
        cancellable_coro,
        rq_function_erased(props1, cancellable_coro, -1, 11),
        rq_function_erased(props2, cancellable_coro, 2, 24))};
    inner_resources inner;
    init_test_inner_service(inner);
    auto tree_ctx = std::make_shared<local_atst_tree_context>(inner);
    auto root_ctx = make_local_async_ctx_tree(tree_ctx, req);

    test_error_async(*root_ctx, req);
}

TEST_CASE("error async request on loopback", tag)
{
    inner_resources inner;
    setup_loopback_test(inner);

    test_error_async_across_rpc(inner, "loopback");
}

TEST_CASE("error async request on rpclib", tag)
{
    inner_resources inner;
    setup_rpclib_test(inner);

    test_error_async_across_rpc(inner, "rpclib");
}

namespace {

// Requests cancellation of all coroutines sharing the context resources
// for ctx
cppcoro::task<void>
checker_coro(async_context_intf& ctx)
{
    auto logger = ensure_logger("checker");
    logger->info("checker_coro(ctx {})", ctx.get_id());
    for (int i = 0; i < 20; ++i)
    {
        auto status = co_await ctx.get_status_coro();
        logger->info("checker_coro {}: {}", i, status);
        if (status == async_status::FINISHED)
        {
            logger->error("finished too early");
            break;
        }
        if (status == async_status::CANCELLED)
        {
            break;
        }
        if (i == 8)
        {
            logger->info("!! checker_coro {}: cancelling", i);
            co_await ctx.request_cancellation_coro();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    co_return;
}

void
checker_func(async_context_intf& ctx)
{
    cppcoro::sync_wait(checker_coro(ctx));
}

template<AsyncContext Ctx, typename Req>
cppcoro::task<void>
test_cancel_async_coro(Ctx& ctx, Req const& req)
{
    REQUIRE_THROWS_AS(co_await resolve_request(ctx, req), async_cancelled);
    REQUIRE(co_await ctx.get_status_coro() == async_status::CANCELLED);
}

template<AsyncContext Ctx, typename Req>
void
test_cancel_async(Ctx& ctx, Req const& req)
{
    // Run the checker coroutine on a separate thread, independent from the
    // ones under test
    // Note: std::thread::~thread() calls terminate() if the thread wasn't
    // joined; e.g. if the test code threw.
    std::jthread checker_thread(checker_func, std::ref(ctx));
    cppcoro::sync_wait(test_cancel_async_coro(ctx, req));
    checker_thread.join();
}

void
test_cancel_async_across_rpc(
    inner_resources& inner, std::string const& proxy_name)
{
    constexpr int loops = 10;
    constexpr auto level = caching_level_type::memory;
    int delay0 = 5;
    int delay1 = 60;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    auto tree_ctx{
        std::make_shared<proxy_atst_tree_context>(inner, proxy_name)};
    auto ctx{root_proxy_atst_context{tree_ctx, true}};

    test_cancel_async(ctx, req);
}

} // namespace

TEST_CASE("cancel async request locally", tag)
{
    using required_ctx_types = ctx_type_list<local_async_context_intf>;
    using Props = request_props<
        caching_level_type::none,
        true,
        false,
        required_ctx_types>;
    Props props0{make_test_uuid(200)};
    Props props1{make_test_uuid(201)};
    auto req{rq_function_erased(
        props0,
        cancellable_coro,
        rq_function_erased(props1, cancellable_coro, 100, 7),
        rq_function_erased(props1, cancellable_coro, 100, 8))};
    inner_resources inner;
    init_test_inner_service(inner);
    auto tree_ctx = std::make_shared<local_atst_tree_context>(inner);
    auto root_ctx = make_local_async_ctx_tree(tree_ctx, req);

    test_cancel_async(*root_ctx, req);
}

TEST_CASE("cancel async request on loopback", tag)
{
    inner_resources inner;
    setup_loopback_test(inner);

    test_cancel_async_across_rpc(inner, "loopback");
}

TEST_CASE("cancel async request on rpclib", tag)
{
    inner_resources inner;
    setup_rpclib_test(inner);

    test_cancel_async_across_rpc(inner, "rpclib");
}

#if 0

// TODO enable these test cases, throw in rpclib_client::submit_async()
namespace {

// Requests cancellation of all coroutines sharing the context resources
// for ctx
cppcoro::task<void>
get_subs_control_coro(async_context_intf& ctx)
{
    auto logger = ensure_logger("checker");
    logger->info("get_subs_control_coro(ctx {})", ctx.get_id());
    auto num_subs = co_await ctx.get_num_subs();
    logger->info("num_subs {}", num_subs);
    co_return;
}

void
get_subs_control_func(async_context_intf& ctx)
{
    REQUIRE_THROWS(cppcoro::sync_wait(get_subs_control_coro(ctx)));
}

template<AsyncContext Ctx, typename Req>
cppcoro::task<void>
test_get_subs_async_coro(Ctx& ctx, Req const& req)
{
    REQUIRE_THROWS(co_await resolve_request(ctx, req));
}

template<AsyncContext Ctx, typename Req>
void
test_get_subs_async(Ctx& ctx, Req const& req)
{
    // Run the checker coroutine on a separate thread, independent from the
    // ones under test
    // Note: std::thread::~thread() calls terminate() if the thread wasn't
    // joined; e.g. if the test code threw.
    std::jthread get_subs_control_thread(get_subs_control_func, std::ref(ctx));
    cppcoro::sync_wait(test_get_subs_async_coro(ctx, req));
    get_subs_control_thread.join();
}

void
test_get_subs_async_across_rpc(
    inner_resources& inner, std::string const& proxy_name)
{
    constexpr int loops = 10;
    constexpr auto level = caching_level_type::memory;
    int delay = 5;
    // Don't need to set props e.g. uuid?!
    auto req{rq_cancellable_coro<level>(loops, delay)};
    auto tree_ctx{
        std::make_shared<proxy_atst_tree_context>(inner, proxy_name)};
    auto ctx{root_proxy_atst_context{tree_ctx, true}};

    test_get_subs_async(ctx, req);
}

} // namespace

TEST_CASE("get subs for request on loopback", tag)
{
    inner_resources inner;
    setup_loopback_test(inner);

    test_get_subs_async_across_rpc(inner, "loopback");
}

TEST_CASE("get subs for request on rpclib", "[B]")
{
    inner_resources inner;
    setup_rpclib_test(inner);

    test_get_subs_async_across_rpc(inner, "rpclib");
}
#endif
