#include <thread>

#include <catch2/catch.hpp>
#include <cppcoro/schedule_on.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <fmt/format.h>

#include "../../support/inner_service.h"
#include <cradle/inner/requests/function.h>
#include <cradle/inner/service/request.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/requests.h>

using namespace cradle;

namespace {

class my_coro_error : public std::runtime_error
{
 public:
    using runtime_error::runtime_error;
};

static char const tag[] = "[inner][service][cancellable]";

request_uuid
make_test_uuid(int ext)
{
    return request_uuid{fmt::format("{}-{:04d}", tag, ext)};
}

cppcoro::task<int>
error_coro(local_async_context_intf& ctx, int x, int delay)
{
    auto logger = spdlog::get("cradle");
    logger->info("error_coro(ctx {}, x={}, delay={})", ctx.get_id(), x, delay);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    auto what{fmt::format("error_coro {}", x)};
    logger->info("error_coro(ctx {}): throwing {}", ctx.get_id(), what);
    throw my_coro_error(what);
    co_return 0;
}

cppcoro::task<int>
simple_coro(local_async_context_intf& ctx, int x, int delay)
{
    auto logger = spdlog::get("cradle");
    logger->info(
        "simple_coro(ctx {}, x={}, delay={})", ctx.get_id(), x, delay);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    logger->info("simple_coro(ctx {}): co_return'ing", ctx.get_id());
    co_return x;
}

// Requests cancellation of all coroutines sharing the context resources
// for ctx
cppcoro::task<int>
killer_coro(local_async_context_intf& ctx)
{
    auto logger = spdlog::get("cradle");
    logger->info("killer_coro(ctx {})", ctx.get_id());
    std::this_thread::sleep_for(std::chrono::milliseconds(42));
    ctx.request_cancellation();
    co_return 0;
}

// Requests cancellation of all coroutines sharing the context resources
// for ctx
cppcoro::task<int>
checker_coro(remote_atst_context& ctx)
{
    auto logger = create_logger("checker");
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
    co_return 0;
}

} // namespace

TEST_CASE("run async coro", tag)
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
    auto tree_ctx = std::make_shared<atst_tree_context>(inner);
    auto root_ctx{make_local_async_ctx_tree(tree_ctx, req)};

    auto res = cppcoro::sync_wait(resolve_request(*root_ctx, req));

    REQUIRE(res == (loops + delay0) + (loops + delay1));
    REQUIRE(root_ctx->get_status() == async_status::FINISHED);
}

TEST_CASE("cancel async coro", tag)
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
    Props props9{make_test_uuid(209)};
    auto killer_req{rq_function_erased(props9, killer_coro)};
    inner_resources inner;
    init_test_inner_service(inner);
    auto tree_ctx = std::make_shared<atst_tree_context>(inner);
    auto normal_root_ctx = make_local_async_ctx_tree(tree_ctx, req);
    auto killer_root_ctx = make_local_async_ctx_tree(tree_ctx, killer_req);

    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(cppcoro::when_all(
            resolve_request(*normal_root_ctx, req),
            resolve_request(*killer_root_ctx, killer_req))),
        cppcoro::operation_cancelled);
    REQUIRE(normal_root_ctx->get_status() == async_status::CANCELLED);
}

TEST_CASE("async error coro", tag)
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
        error_coro,
        rq_function_erased(props1, error_coro, 1, 11),
        rq_function_erased(props2, simple_coro, 2, 24))};
    inner_resources inner;
    init_test_inner_service(inner);
    auto tree_ctx = std::make_shared<atst_tree_context>(inner);
    auto root_ctx = make_local_async_ctx_tree(tree_ctx, req);

    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(resolve_request(*root_ctx, req)), my_coro_error);
    REQUIRE(root_ctx->get_status() == async_status::ERROR);
}

TEST_CASE("resolve async across rpc", "[X]")
{
    inner_resources inner;
    init_test_inner_service(inner);
    ensure_rpclib_service();
    constexpr int loops = 3;
    constexpr auto level = caching_level_type::memory;
    int delay0 = 5;
    int delay1 = 60;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    auto ctx{make_remote_async_ctx()};

    auto res = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res == (loops + delay0) + (loops + delay1));
    REQUIRE(ctx.is_req());
    REQUIRE(ctx.get_status() == async_status::FINISHED);
    REQUIRE(ctx.get_num_subs() == 2);
    auto& ctx0 = ctx.get_sub(0);
    REQUIRE(ctx.is_req());
    REQUIRE(ctx0.get_num_subs() == 2);
    REQUIRE(ctx0.get_status() == async_status::FINISHED);
    auto& ctx00 = ctx0.get_sub(0);
    REQUIRE(ctx00.is_req());
    REQUIRE(ctx00.get_num_subs() == 1);
    REQUIRE(ctx00.get_status() == async_status::FINISHED);
    auto& ctx000 = ctx00.get_sub(0);
    REQUIRE(!ctx000.is_req());
    REQUIRE(ctx000.get_status() == async_status::FINISHED);
    auto& ctx01 = ctx0.get_sub(1);
    REQUIRE(ctx01.is_req());
    REQUIRE(ctx01.get_status() == async_status::FINISHED);
    auto& ctx010 = ctx01.get_sub(0);
    REQUIRE(!ctx010.is_req());
    REQUIRE(ctx010.get_status() == async_status::FINISHED);
    auto& ctx1 = ctx.get_sub(1);
    REQUIRE(ctx.is_req());
    REQUIRE(ctx1.get_num_subs() == 2);
    REQUIRE(ctx1.get_status() == async_status::FINISHED);
    auto& ctx10 = ctx1.get_sub(0);
    REQUIRE(ctx10.is_req());
    REQUIRE(ctx10.get_num_subs() == 1);
    REQUIRE(ctx10.get_status() == async_status::FINISHED);
    auto& ctx100 = ctx10.get_sub(0);
    REQUIRE(!ctx100.is_req());
    REQUIRE(ctx100.get_status() == async_status::FINISHED);
    auto& ctx11 = ctx1.get_sub(1);
    REQUIRE(ctx11.is_req());
    REQUIRE(ctx11.get_status() == async_status::FINISHED);
    auto& ctx110 = ctx11.get_sub(0);
    REQUIRE(!ctx110.is_req());
    REQUIRE(ctx110.get_status() == async_status::FINISHED);
}

// TODO server's disk cache must be cleared before running this
TEST_CASE("test async across rpc", "[Y]")
{
    inner_resources inner;
    init_test_inner_service(inner);
    ensure_rpclib_service();
    constexpr int loops = 10;
    constexpr auto level = caching_level_type::memory;
    int delay0 = 5;
    int delay1 = 60;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    auto ctx{make_remote_async_ctx()};

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(
            cppcoro::when_all(resolve_request(ctx, req), checker_coro(ctx))),
        Catch::Equals("remote async cancelled"));
    REQUIRE(ctx.get_status() == async_status::CANCELLED);
}
