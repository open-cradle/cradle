#include <benchmark/benchmark.h>

#include <cradle/inner/requests/value.h>

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
BM_call_value_request_resolve(benchmark::State& state)
{
    auto resources{make_inner_test_resources()};
    for (auto _ : state)
    {
        call_resolve_by_ref_loop(rq_value(42), *resources);
    }
}
BENCHMARK(BM_call_value_request_resolve)->Apply(thousand_loops);

static void
BM_resolve_value_request(benchmark::State& state)
{
    auto resources{make_inner_test_resources()};
    non_caching_request_resolution_context ctx{*resources};
    for (auto _ : state)
    {
        resolve_request_loop(state, ctx, rq_value(42));
    }
}
BENCHMARK(BM_resolve_value_request)->Apply(thousand_loops);
