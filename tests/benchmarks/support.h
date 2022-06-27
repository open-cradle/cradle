#ifndef CRADLE_TESTS_BENCHMARKS_SUPPORT_H
#define CRADLE_TESTS_BENCHMARKS_SUPPORT_H

#include <cradle/inner/service/request.h>

namespace cradle {

template<UncachedRequest Req>
auto
call_resolve_by_ref_loop(Req const& req, int expected)
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

template<UncachedRequestPtr Req>
auto
call_resolve_by_ptr_loop(Req const& req, int expected)
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

template<UncachedRequestOrPtr Req>
auto
resolve_request_loop(Req const& req, int expected)
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

template<CachedRequestOrPtr Req>
auto
resolve_request_loop(Req const& req, int expected)
{
    constexpr int num_loops = 1000;
    cached_request_resolution_context ctx{};
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

} // namespace cradle

#endif
