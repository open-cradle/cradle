#ifndef CRADLE_TESTS_BENCHMARKS_SUPPORT_H
#define CRADLE_TESTS_BENCHMARKS_SUPPORT_H

#include <cradle/inner/service/request.h>

template<typename Request>
auto
call_resolve_by_ref_loop(Request const& req, int expected)
{
    constexpr int num_loops = 1000;
    uncached_request_resolution_context ctx{};
    auto loop = [&]() -> cppcoro::task<int> {
        int total{};
        for (auto i = 0; i < num_loops; ++i)
        {
            total += co_await req.resolve(ctx);
        }
        co_return total;
    };
    auto actual = cppcoro::sync_wait(loop());
    REQUIRE(actual == expected * num_loops);
}

template<typename Request>
auto
call_resolve_by_ptr_loop(Request const& req, int expected)
{
    constexpr int num_loops = 1000;
    uncached_request_resolution_context ctx{};
    auto loop = [&]() -> cppcoro::task<int> {
        int total{};
        for (auto i = 0; i < num_loops; ++i)
        {
            total += co_await req->resolve(ctx);
        }
        co_return total;
    };
    auto actual = cppcoro::sync_wait(loop());
    REQUIRE(actual == expected * num_loops);
}

template<typename Request>
auto
resolve_request_loop(Request const& req, int expected)
{
    constexpr int num_loops = 1000;
    uncached_request_resolution_context ctx{};
    auto loop = [&]() -> cppcoro::task<int> {
        int total{};
        for (auto i = 0; i < num_loops; ++i)
        {
            total += co_await resolve_request(ctx, req);
        }
        co_return total;
    };
    auto actual = cppcoro::sync_wait(loop());
    REQUIRE(actual == expected * num_loops);
}

#endif
