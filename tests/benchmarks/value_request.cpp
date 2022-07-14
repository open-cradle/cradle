#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/value.h>
#include <cradle/inner/service/core.h>

#include "../inner/support/core.h"
#include "support.h"

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
BENCHMARK(BM_call_value_request_resolve)->Arg(1000);

static void
BM_call_value_request_up_resolve(benchmark::State& state)
{
    for (auto _ : state)
    {
        call_resolve_by_ptr_loop(rq_value_up(42));
    }
}
BENCHMARK(BM_call_value_request_up_resolve)->Arg(1000);

static void
BM_call_value_request_sp_resolve(benchmark::State& state)
{
    for (auto _ : state)
    {
        call_resolve_by_ptr_loop(rq_value_sp(42));
    }
}
BENCHMARK(BM_call_value_request_sp_resolve)->Arg(1000);

static void
BM_resolve_value_request(benchmark::State& state)
{
    for (auto _ : state)
    {
        resolve_request_loop(rq_value(42));
    }
}
BENCHMARK(BM_resolve_value_request)->Arg(1000);

static void
BM_resolve_value_request_up(benchmark::State& state)
{
    for (auto _ : state)
    {
        resolve_request_loop(rq_value_up(42));
    }
}
BENCHMARK(BM_resolve_value_request_up)->Arg(1000);

static void
BM_resolve_value_request_sp(benchmark::State& state)
{
    for (auto _ : state)
    {
        resolve_request_loop(rq_value_sp(42));
    }
}
BENCHMARK(BM_resolve_value_request_sp)->Arg(1000);
