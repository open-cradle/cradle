#ifndef CRADLE_TESTS_BENCHMARKS_BENCHMARK_SUPPORT_H
#define CRADLE_TESTS_BENCHMARKS_BENCHMARK_SUPPORT_H

#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include "../support/concurrency_testing.h"
#include "../support/inner_service.h"
#include <cradle/inner/resolve/resolve_request.h>

namespace cradle {

/*
 * Handles an exception thrown by a benchmark: marks the benchmark as skipped,
 * and causes the program to end with an exit code.
 */
void
handle_benchmark_exception(benchmark::State& state, std::string const& what);

/*
 * Reports a summary on benchmarks skipped with error.
 * Returns intended main() exit code.
 * Note that the benchmark library itself does not offer this functionality.
 */
int
check_benchmarks_skipped_with_error();

template<UncachedRequest Req>
void
call_resolve_by_ref_loop(Req const& req, inner_resources& resources)
{
    constexpr int num_loops = 1000;
    non_caching_request_resolution_context ctx{resources};
    auto loop = [&]() -> cppcoro::task<int> {
        int total{};
        for (auto i = 0; i < num_loops; ++i)
        {
            total += co_await req.resolve_sync(ctx);
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
void
resolve_request_loop(
    benchmark::State& state, Ctx& ctx, Req const& req, int num_loops = 1000)
{
    auto& resources{ctx.get_resources()};
    constexpr auto caching_level = Req::element_type::caching_level;
    auto loop = [&]() -> cppcoro::task<void> {
        if constexpr (is_fully_cached(caching_level))
        {
            state.PauseTiming();
            resources.reset_memory_cache();
            benchmark::DoNotOptimize(co_await resolve_request(ctx, req));
            sync_wait_write_disk_cache(ctx.get_resources());
            state.ResumeTiming();
        }
        for (int i = 0; i < num_loops; ++i)
        {
            if constexpr (is_fully_cached(caching_level))
            {
                state.PauseTiming();
                resources.reset_memory_cache();
                state.ResumeTiming();
            }
            benchmark::DoNotOptimize(co_await resolve_request(ctx, req));
        }
        co_return;
    };
    cppcoro::sync_wait(loop());
}

template<typename Ctx, Request Req>
void
BM_resolve_request(benchmark::State& state, Ctx& ctx, Req const& req)
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
