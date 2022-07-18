#ifndef CRADLE_TESTS_BENCHMARKS_SUPPORT_H
#define CRADLE_TESTS_BENCHMARKS_SUPPORT_H

#include "../inner/support/concurrency_testing.h"
#include <cradle/inner/service/request.h>

namespace cradle {

template<UncachedRequest Req>
auto
call_resolve_by_ref_loop(Req const& req)
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
    cppcoro::sync_wait(loop());
}

template<UncachedRequestPtr Req>
auto
call_resolve_by_ptr_loop(Req const& req)
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
    cppcoro::sync_wait(loop());
}

template<typename Ctx, RequestOrPtr Req>
requires(MatchingContextRequest<
         Ctx,
         typename Req::
             element_type>) auto resolve_request_loop(Ctx& ctx, Req const& req)
{
    constexpr int num_loops = 1000;
    auto loop = [&]() -> cppcoro::task<int> {
        int total{};
        for (auto i = 0; i < num_loops; ++i)
        {
            total += co_await resolve_request(ctx, req);
        }
        co_return total;
    };
    cppcoro::sync_wait(loop());
}

template<CachedRequest Req>
requires(Req::caching_level == caching_level_type::full) auto resolve_request_loop_full(
    cached_request_resolution_context& ctx, Req const& req)
{
    constexpr int num_loops = 1000;
    auto loop = [&]() -> cppcoro::task<int> {
        ctx.reset_memory_cache();
        int total{co_await resolve_request(ctx, req)};
        sync_wait_write_disk_cache(ctx.get_service());
        for (auto i = 1; i < num_loops; ++i)
        {
            ctx.reset_memory_cache();
            total += co_await resolve_request(ctx, req);
        }
        co_return total;
    };
    cppcoro::sync_wait(loop());
}

} // namespace cradle

#endif
