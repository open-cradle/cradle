#include <functional>
#include <thread>

#include <catch2/catch.hpp>
#include <cppcoro/schedule_on.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <fmt/format.h>

#include "../../support/inner_service.h"
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/service/request.h>
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
    register_and_initialize_testing_domains();
}

void
setup_rpclib_test(inner_resources& inner)
{
    init_test_inner_service(inner);
    ensure_rpclib_service();
}

template<typename Ctx, typename Req>
cppcoro::task<void>
test_resolve_async_coro(
    Ctx& ctx,
    Req const& req,
    bool requests_are_normalized,
    int loops,
    int delay0,
    int delay1)
{
    auto res = co_await resolve_request(ctx, req);

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

template<typename Ctx, typename Req>
void
test_resolve_async(
    Ctx& ctx,
    Req const& req,
    bool requests_are_normalized,
    int loops,
    int delay0,
    int delay1)
{
    cppcoro::sync_wait(test_resolve_async_coro(
        ctx, req, requests_are_normalized, loops, delay0, delay1));
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
    auto ctx{make_remote_async_ctx(tree_ctx)};

    test_resolve_async(ctx, req, true, loops, delay0, delay1);
}

} // namespace

TEST_CASE("resolve async locally", tag)
{
    constexpr int loops = 3;
    using Props = request_props<
        caching_level_type::none,
        true,
        false,
        local_async_context_intf>;
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

    test_resolve_async(*root_ctx, req, false, loops, delay0, delay1);
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
    auto ctx{make_remote_async_ctx(tree_ctx)};

    test_error_async(ctx, req);
}

} // namespace

TEST_CASE("error async request locally", tag)
{
    using Props = request_props<
        caching_level_type::none,
        true,
        false,
        local_async_context_intf>;
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
    std::thread checker_thread(checker_func, std::ref(ctx));
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
    auto ctx{make_remote_async_ctx(tree_ctx)};

    test_cancel_async(ctx, req);
}

} // namespace

TEST_CASE("cancel async request locally", tag)
{
    using Props = request_props<
        caching_level_type::none,
        true,
        false,
        local_async_context_intf>;
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
