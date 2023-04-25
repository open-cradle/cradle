#ifndef CRADLE_TESTS_BENCHMARKS_BENCHMARK_SUPPORT_H
#define CRADLE_TESTS_BENCHMARKS_BENCHMARK_SUPPORT_H

#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include "../support/concurrency_testing.h"
#include <cradle/inner/service/request.h>

namespace cradle {

template<UncachedRequest Req>
void
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

// The purpose of the loop is to bring the sync_wait() overhead down to
// amortized zero. However, each time the same request is resolved, meaning
// - Any calculations cached inside the request itself (e.g., its hash), are
// not measured
// - Each equals() will immediately return true, so is also not really measured
template<typename Ctx, Request Req>
requires(ContextMatchingRequest<Ctx, typename Req::element_type>) void resolve_request_loop(
    benchmark::State& state, Ctx& ctx, Req const& req, int num_loops = 1000)
{
    constexpr auto caching_level = Req::element_type::caching_level;
    auto loop = [&]() -> cppcoro::task<void> {
        if constexpr (caching_level == caching_level_type::full)
        {
            state.PauseTiming();
            ctx.reset_memory_cache();
            benchmark::DoNotOptimize(co_await resolve_request(ctx, req));
            sync_wait_write_disk_cache(ctx.get_resources());
            state.ResumeTiming();
        }
        for (int i = 0; i < num_loops; ++i)
        {
            if constexpr (caching_level == caching_level_type::full)
            {
                state.PauseTiming();
                ctx.reset_memory_cache();
                state.ResumeTiming();
            }
            benchmark::DoNotOptimize(co_await resolve_request(ctx, req));
        }
        co_return;
    };
    cppcoro::sync_wait(loop());
}

template<typename Ctx, Request Req>
requires(ContextMatchingRequest<Ctx, typename Req::element_type>) void BM_resolve_request(
    benchmark::State& state, Ctx& ctx, Req const& req)
{
    for (auto _ : state)
    {
        resolve_request_loop(state, ctx, req);
    }
}

// 1000 inner loops bring the sync_wait() overhead down to amortized zero.
// The reported "us" should be interpreted as "ns" for one resolve.
inline void
thousand_loops(benchmark::internal::Benchmark* b)
{
    b->Arg(1000)->Unit(benchmark::kMicrosecond);
}

} // namespace cradle

#endif
