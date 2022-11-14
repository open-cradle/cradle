#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/value.h>
#include <cradle/inner/service/resources.h>

#include "../support/inner_service.h"
#include "benchmark_support.h"

using namespace cradle;

static void
BM_create_value_request(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(cradle::rq_value(42));
    }
}
BENCHMARK(BM_create_value_request);

static void
BM_create_value_request_up(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(cradle::rq_value_up(42));
    }
}
BENCHMARK(BM_create_value_request_up);

static void
BM_create_value_request_sp(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(cradle::rq_value_sp(42));
    }
}
BENCHMARK(BM_create_value_request_sp);

static void
BM_call_value_request_resolve(benchmark::State& state)
{
    for (auto _ : state)
    {
        call_resolve_by_ref_loop(rq_value(42));
    }
}
BENCHMARK(BM_call_value_request_resolve)->Apply(thousand_loops);

static void
BM_call_value_request_up_resolve(benchmark::State& state)
{
    for (auto _ : state)
    {
        call_resolve_by_ptr_loop(rq_value_up(42));
    }
}
BENCHMARK(BM_call_value_request_up_resolve)->Apply(thousand_loops);

static void
BM_call_value_request_sp_resolve(benchmark::State& state)
{
    for (auto _ : state)
    {
        call_resolve_by_ptr_loop(rq_value_sp(42));
    }
}
BENCHMARK(BM_call_value_request_sp_resolve)->Apply(thousand_loops);

static void
BM_resolve_value_request(benchmark::State& state)
{
    uncached_request_resolution_context ctx;
    for (auto _ : state)
    {
        resolve_request_loop(state, ctx, rq_value(42));
    }
}
BENCHMARK(BM_resolve_value_request)->Apply(thousand_loops);

static void
BM_resolve_value_request_up(benchmark::State& state)
{
    uncached_request_resolution_context ctx;
    for (auto _ : state)
    {
        resolve_request_loop(state, ctx, rq_value_up(42));
    }
}
BENCHMARK(BM_resolve_value_request_up)->Apply(thousand_loops);

static void
BM_resolve_value_request_sp(benchmark::State& state)
{
    uncached_request_resolution_context ctx;
    for (auto _ : state)
    {
        resolve_request_loop(state, ctx, rq_value_sp(42));
    }
}
BENCHMARK(BM_resolve_value_request_sp)->Apply(thousand_loops);
