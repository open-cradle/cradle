#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/serialization/disk_cache/preferred/cereal/cereal.h>

#include "../support/inner_service.h"
#include "benchmark_support.h"

using namespace cradle;

static auto add = [](int a, int b) { return a + b; };

namespace {

request_uuid
make_uuid()
{
    static int next_id{};
    return request_uuid{fmt::format("benchmark-{}", next_id++)};
}

template<int H>
auto
create_thin_tree()
{
    auto constexpr level = caching_level_type::none;
    if constexpr (H == 1)
    {
        return rq_function<level>(add, 2, 1);
    }
    else
    {
        return rq_function<level>(add, create_thin_tree<H - 1>(), 1);
    }
}

template<caching_level_type level, int H>
auto
create_triangular_tree()
{
    if constexpr (H == 1)
    {
        return rq_function<level>(add, 2, 1);
    }
    else
    {
        return rq_function<level>(
            add,
            create_triangular_tree<level, H - 1>(),
            create_triangular_tree<level, H - 1>());
    }
}

} // namespace

template<int H>
void
BM_create_thin_tree(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(create_thin_tree<H>());
    }
}

template<int H>
void
BM_create_triangular_tree(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(
            create_triangular_tree<caching_level_type::none, H>());
    }
}

BENCHMARK(BM_create_thin_tree<2>)
    ->Name("BM_create_function_request_thin_tree H=2");
BENCHMARK(BM_create_thin_tree<4>)
    ->Name("BM_create_function_request_thin_tree H=4");
BENCHMARK(BM_create_thin_tree<16>)
    ->Name("BM_create_function_request_thin_tree H=16");
BENCHMARK(BM_create_thin_tree<64>)
    ->Name("BM_create_function_request_thin_tree H=64");

BENCHMARK(BM_create_triangular_tree<2>)
    ->Name("BM_create_function_request_tri_tree H=2");
BENCHMARK(BM_create_triangular_tree<4>)
    ->Name("BM_create_function_request_tri_tree H=4");
BENCHMARK(BM_create_triangular_tree<6>)
    ->Name("BM_create_function_request_tri_tree H=6");

template<int H>
auto
create_thin_tree_up()
{
    auto constexpr level = caching_level_type::none;
    if constexpr (H == 1)
    {
        return rq_function_up<level>(add, rq_value_up(2), rq_value_up(1));
    }
    else
    {
        return rq_function_up<level>(
            add, create_thin_tree_up<H - 1>(), rq_value_up(1));
    }
}

template<int H>
auto
create_triangular_tree_up()
{
    auto constexpr level = caching_level_type::none;
    if constexpr (H == 1)
    {
        return rq_function_up<level>(add, rq_value_up(2), rq_value_up(1));
    }
    else
    {
        return rq_function_up<level>(
            add,
            create_triangular_tree_up<H - 1>(),
            create_triangular_tree_up<H - 1>());
    }
}

template<int H>
void
BM_create_thin_tree_up(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(create_thin_tree_up<H>());
    }
}

BENCHMARK(BM_create_thin_tree_up<2>)
    ->Name("BM_create_function_request_up_thin_tree_H2");
BENCHMARK(BM_create_thin_tree_up<4>)
    ->Name("BM_create_function_request_up_thin_tree_H4");
// Taller trees tend to blow up the compilers.

template<int H>
void
BM_create_triangular_tree_up(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(create_triangular_tree_up<H>());
    }
}

BENCHMARK(BM_create_triangular_tree_up<2>)
    ->Name("BM_create_function_request_up_tri_tree H=2");
BENCHMARK(BM_create_triangular_tree_up<4>)
    ->Name("BM_create_function_request_up_tri_tree H=4");
BENCHMARK(BM_create_triangular_tree_up<6>)
    ->Name("BM_create_function_request_up_tri_tree H=6");

template<int H>
auto
create_thin_tree_sp()
{
    auto constexpr level = caching_level_type::none;
    if constexpr (H == 1)
    {
        return rq_function_sp<level>(add, rq_value_sp(2), rq_value_sp(1));
    }
    else
    {
        return rq_function_sp<level>(
            add, create_thin_tree_sp<H - 1>(), rq_value_sp(1));
    }
}

template<int H>
void
BM_create_thin_tree_sp(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(create_thin_tree_sp<H>());
    }
}

BENCHMARK(BM_create_thin_tree_sp<2>)
    ->Name("BM_create_function_request_sp_thin_tree H=2");
BENCHMARK(BM_create_thin_tree_sp<4>)
    ->Name("BM_create_function_request_sp_thin_tree H=4");
BENCHMARK(BM_create_thin_tree_sp<16>)
    ->Name("BM_create_function_request_sp_thin_tree H=16");
BENCHMARK(BM_create_thin_tree_sp<64>)
    ->Name("BM_create_function_request_sp_thin_tree H=64");

template<int H>
auto
create_triangular_tree_sp()
{
    auto constexpr level = caching_level_type::none;
    if constexpr (H == 1)
    {
        return rq_function_sp<level>(add, rq_value_sp(2), rq_value_sp(1));
    }
    else
    {
        return rq_function_sp<level>(
            add,
            create_triangular_tree_sp<H - 1>(),
            create_triangular_tree_sp<H - 1>());
    }
}

template<int H>
void
BM_create_triangular_tree_sp(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(create_triangular_tree_sp<H>());
    }
}

BENCHMARK(BM_create_triangular_tree_sp<2>)
    ->Name("BM_create_function_request_sp_tri_tree H=2");
BENCHMARK(BM_create_triangular_tree_sp<4>)
    ->Name("BM_create_function_request_sp_tri_tree H=4");
BENCHMARK(BM_create_triangular_tree_sp<6>)
    ->Name("BM_create_function_request_sp_tri_tree H=6");

template<int H>
auto
create_thin_tree_mixed()
{
    auto constexpr level = caching_level_type::none;
    if constexpr (H == 1)
    {
        return rq_function<level>(add, 2, 1);
    }
    else if constexpr (H == 2)
    {
        return rq_function<level>(add, create_thin_tree_mixed<H - 1>(), 1);
    }
    else
    {
        return rq_function_sp<level>(add, create_thin_tree_mixed<H - 1>(), 1);
    }
}

template<int H>
void
BM_create_thin_tree_mixed(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(create_thin_tree_mixed<H>());
    }
}

BENCHMARK(BM_create_thin_tree_mixed<2>)
    ->Name("BM_create_function_request_mixed_thin_tree H=2");
BENCHMARK(BM_create_thin_tree_mixed<4>)
    ->Name("BM_create_function_request_mixed_thin_tree H=4");
BENCHMARK(BM_create_thin_tree_mixed<16>)
    ->Name("BM_create_function_request_mixed_thin_tree H=16");
BENCHMARK(BM_create_thin_tree_mixed<64>)
    ->Name("BM_create_function_request_mixed_thin_tree H=64");

template<int H>
auto
create_triangular_tree_mixed()
{
    auto constexpr level = caching_level_type::none;
    if constexpr (H == 1)
    {
        return rq_function<level>(add, 2, 1);
    }
    else if constexpr (H == 2)
    {
        return rq_function<level>(
            add,
            create_triangular_tree_mixed<H - 1>(),
            create_triangular_tree_mixed<H - 1>());
    }
    else
    {
        auto subreq{create_triangular_tree_mixed<H - 1>()};
        return rq_function_sp<level>(add, subreq, subreq);
    }
}

template<int H>
void
BM_create_triangular_tree_mixed(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(create_triangular_tree_mixed<H>());
    }
}

BENCHMARK(BM_create_triangular_tree_mixed<2>)
    ->Name("BM_create_function_request_mixed_tri_tree H=2");
BENCHMARK(BM_create_triangular_tree_mixed<4>)
    ->Name("BM_create_function_request_mixed_tri_tree H=4");
BENCHMARK(BM_create_triangular_tree_mixed<6>)
    ->Name("BM_create_function_request_mixed_tri_tree H=6");

template<caching_level_type level, int H>
auto
create_thin_tree_erased()
{
    request_props<level> props{make_uuid()};
    if constexpr (H == 1)
    {
        return rq_function_erased(props, add, 2, 1);
    }
    else
    {
        return rq_function_erased(
            props, add, create_thin_tree_erased<level, H - 1>(), 1);
    }
}

template<caching_level_type level, int H>
    requires(level != caching_level_type::full)
auto create_triangular_tree_erased()
{
    request_props<level> props{make_uuid()};
    if constexpr (H == 1)
    {
        return rq_function_erased(props, add, 2, 1);
    }
    else
    {
        return rq_function_erased(
            props,
            add,
            create_triangular_tree_erased<level, H - 1>(),
            create_triangular_tree_erased<level, H - 1>());
    }
}

template<caching_level_type level, int H>
    requires(level == caching_level_type::full)
auto create_triangular_tree_erased()
{
    request_props<level, false, false> props{make_uuid()};
    if constexpr (H == 1)
    {
        return rq_function_erased(props, add, 2, 1);
    }
    else
    {
        return rq_function_erased(
            props,
            add,
            create_triangular_tree_erased<level, H - 1>(),
            create_triangular_tree_erased<level, H - 1>());
    }
}

template<caching_level_type level, int H>
auto
create_triangular_tree_erased_introspective()
{
    if constexpr (H == 1)
    {
        request_props<level, false, true> props{make_uuid(), "add 2+1"};
        return rq_function_erased(props, add, 2, 1);
    }
    else
    {
        std::string title{fmt::format("add H{}", H)};
        request_props<level, false, true> props{make_uuid(), title};
        return rq_function_erased(
            props,
            add,
            create_triangular_tree_erased_introspective<level, H - 1>(),
            create_triangular_tree_erased_introspective<level, H - 1>());
    }
}

template<caching_level_type level, int H>
void
BM_create_thin_tree_erased(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(create_thin_tree_erased<level, H>());
    }
}

template<caching_level_type level, int H>
void
BM_create_triangular_tree_erased(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(create_triangular_tree_erased<level, H>());
    }
}

BENCHMARK(BM_create_thin_tree_erased<caching_level_type::none, 2>)
    ->Name("BM_create_function_request_erased_uncached_thin_tree H=2");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::none, 4>)
    ->Name("BM_create_function_request_erased_uncached_thin_tree H=4");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::none, 16>)
    ->Name("BM_create_function_request_erased_uncached_thin_tree H=16");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::none, 64>)
    ->Name("BM_create_function_request_erased_uncached_thin_tree H=64");

BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::none, 2>)
    ->Name("BM_create_function_request_erased_uncached_tri_tree H=2");
BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::none, 4>)
    ->Name("BM_create_function_request_erased_uncached_tri_tree H=4");
BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::none, 6>)
    ->Name("BM_create_function_request_erased_uncached_tri_tree H=6");

BENCHMARK(BM_create_thin_tree_erased<caching_level_type::memory, 2>)
    ->Name("BM_create_function_request_erased_cached_thin_tree H=2");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::memory, 4>)
    ->Name("BM_create_function_request_erased_cached_thin_tree H=4");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::memory, 16>)
    ->Name("BM_create_function_request_erased_cached_thin_tree H=16");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::memory, 64>)
    ->Name("BM_create_function_request_erased_cached_thin_tree H=64");

BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::memory, 2>)
    ->Name("BM_create_function_request_erased_cached_tri_tree H=2");
BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::memory, 4>)
    ->Name("BM_create_function_request_erased_cached_tri_tree H=4");
BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::memory, 6>)
    ->Name("BM_create_function_request_erased_cached_tri_tree H=6");

template<caching_level_type level, int H>
void
BM_create_tri_tree_erased_intrsp(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(
            create_triangular_tree_erased_introspective<level, H>());
    }
}

BENCHMARK(BM_create_tri_tree_erased_intrsp<caching_level_type::memory, 4>)
    ->Name("BM_create_function_request_erased_cached_intrsp_tri_tree H=4");
BENCHMARK(BM_create_tri_tree_erased_intrsp<caching_level_type::memory, 6>)
    ->Name("BM_create_function_request_erased_cached_intrsp_tri_tree H=6");

template<int H>
void
BM_resolve_thin_tree(benchmark::State& state)
{
    uncached_request_resolution_context ctx{};
    BM_resolve_request(state, ctx, create_thin_tree<H>());
}

BENCHMARK(BM_resolve_thin_tree<2>)
    ->Name("BM_resolve_function_request_thin_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree<4>)
    ->Name("BM_resolve_function_request_thin_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree<16>)
    ->Name("BM_resolve_function_request_thin_tree H=16")
    ->Apply(thousand_loops);
#ifndef _MSC_VER
// VS2019 internal compiler error
BENCHMARK(BM_resolve_thin_tree<64>)
    ->Name("BM_resolve_function_request_thin_tree H=64")
    ->Apply(thousand_loops);
#endif

template<caching_level_type level, int H>
void
BM_resolve_triangular_tree(benchmark::State& state)
{
    request_resolution_context<level> ctx{};
    BM_resolve_request(state, ctx, create_triangular_tree<level, H>());
}

BENCHMARK(BM_resolve_triangular_tree<caching_level_type::none, 2>)
    ->Name("BM_resolve_function_request_uncached_tri_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree<caching_level_type::none, 4>)
    ->Name("BM_resolve_function_request_uncached_tri_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree<caching_level_type::none, 6>)
    ->Name("BM_resolve_function_request_uncached_tri_tree H=6")
    ->Apply(thousand_loops);

BENCHMARK(BM_resolve_triangular_tree<caching_level_type::memory, 2>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree<caching_level_type::memory, 4>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree<caching_level_type::memory, 6>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=6")
    ->Apply(thousand_loops);

template<int H>
void
BM_resolve_thin_tree_up(benchmark::State& state)
{
    uncached_request_resolution_context ctx{};
    BM_resolve_request(state, ctx, create_thin_tree_up<H>());
}

BENCHMARK(BM_resolve_thin_tree_up<2>)
    ->Name("BM_resolve_function_request_up_thin_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_up<4>)
    ->Name("BM_resolve_function_request_up_thin_tree H=4")
    ->Apply(thousand_loops);
// Taller trees tend to blow up the compilers.

template<int H>
void
BM_resolve_triangular_tree_up(benchmark::State& state)
{
    uncached_request_resolution_context ctx{};
    BM_resolve_request(state, ctx, create_triangular_tree_up<H>());
}

BENCHMARK(BM_resolve_triangular_tree_up<2>)
    ->Name("BM_resolve_function_request_up_tri_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree_up<4>)
    ->Name("BM_resolve_function_request_up_tri_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree_up<6>)
    ->Name("BM_resolve_function_request_up_tri_tree H=6")
    ->Apply(thousand_loops);

template<int H>
void
BM_resolve_thin_tree_sp(benchmark::State& state)
{
    uncached_request_resolution_context ctx{};
    BM_resolve_request(state, ctx, create_thin_tree_sp<H>());
}

BENCHMARK(BM_resolve_thin_tree_sp<2>)
    ->Name("BM_resolve_function_request_sp_thin_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_sp<4>)
    ->Name("BM_resolve_function_request_sp_thin_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_sp<16>)
    ->Name("BM_resolve_function_request_sp_thin_tree H=16")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_sp<64>)
    ->Name("BM_resolve_function_request_sp_thin_tree H=64")
    ->Apply(thousand_loops);

template<int H>
void
BM_resolve_triangular_tree_sp(benchmark::State& state)
{
    uncached_request_resolution_context ctx{};
    BM_resolve_request(state, ctx, create_triangular_tree_sp<H>());
}

BENCHMARK(BM_resolve_triangular_tree_sp<2>)
    ->Name("BM_resolve_function_request_sp_tri_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree_sp<4>)
    ->Name("BM_resolve_function_request_sp_tri_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree_sp<6>)
    ->Name("BM_resolve_function_request_sp_tri_tree H=6")
    ->Apply(thousand_loops);

template<int H>
void
BM_resolve_thin_tree_mixed(benchmark::State& state)
{
    uncached_request_resolution_context ctx{};
    BM_resolve_request(state, ctx, create_thin_tree_mixed<H>());
}

BENCHMARK(BM_resolve_thin_tree_mixed<2>)
    ->Name("BM_resolve_function_request_mixed_thin_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_mixed<4>)
    ->Name("BM_resolve_function_request_mixed_thin_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_mixed<16>)
    ->Name("BM_resolve_function_request_mixed_thin_tree H=16")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_mixed<64>)
    ->Name("BM_resolve_function_request_mixed_thin_tree H=64")
    ->Apply(thousand_loops);

template<int H>
void
BM_resolve_triangular_tree_mixed(benchmark::State& state)
{
    uncached_request_resolution_context ctx{};
    BM_resolve_request(state, ctx, create_triangular_tree_mixed<H>());
}

BENCHMARK(BM_resolve_triangular_tree_mixed<2>)
    ->Name("BM_resolve_function_request_mixed_tri_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree_mixed<4>)
    ->Name("BM_resolve_function_request_mixed_tri_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree_mixed<6>)
    ->Name("BM_resolve_function_request_mixed_tri_tree H=6")
    ->Apply(thousand_loops);

template<caching_level_type level, int H>
void
BM_resolve_thin_tree_erased(benchmark::State& state)
{
    request_resolution_context<level> ctx{};
    BM_resolve_request(state, ctx, create_thin_tree_erased<level, H>());
}

BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::none, 2>)
    ->Name("BM_resolve_function_request_erased_uncached_thin_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::none, 4>)
    ->Name("BM_resolve_function_request_erased_uncached_thin_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::none, 16>)
    ->Name("BM_resolve_function_request_erased_uncached_thin_tree H=16")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::none, 64>)
    ->Name("BM_resolve_function_request_erased_uncached_thin_tree H=64")
    ->Apply(thousand_loops);

template<caching_level_type level, int H>
void
BM_resolve_tri_tree_erased(benchmark::State& state)
{
    request_resolution_context<level> ctx{};
    BM_resolve_request(state, ctx, create_triangular_tree_erased<level, H>());
}

BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::none, 2>)
    ->Name("BM_resolve_function_request_erased_uncached_tri_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::none, 4>)
    ->Name("BM_resolve_function_request_erased_uncached_tri_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::none, 6>)
    ->Name("BM_resolve_function_request_erased_uncached_tri_tree H=6")
    ->Apply(thousand_loops);

BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::memory, 2>)
    ->Name("BM_resolve_function_request_erased_mem_cached_thin_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::memory, 4>)
    ->Name("BM_resolve_function_request_erased_mem_cached_thin_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::memory, 16>)
    ->Name("BM_resolve_function_request_erased_mem_cached_thin_tree H=16")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::memory, 64>)
    ->Name("BM_resolve_function_request_erased_mem_cached_thin_tree H=64")
    ->Apply(thousand_loops);

BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory, 2>)
    ->Name("BM_resolve_function_request_erased_mem_cached_tri_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory, 4>)
    ->Name("BM_resolve_function_request_erased_mem_cached_tri_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory, 6>)
    ->Name("BM_resolve_function_request_erased_mem_cached_tri_tree H=6")
    ->Apply(thousand_loops);

template<int H>
void
BM_resolve_triangular_tree_erased_full(benchmark::State& state)
{
    spdlog::set_level(spdlog::level::warn);
    cached_request_resolution_context ctx{};
    BM_resolve_request(
        state,
        ctx,
        create_triangular_tree_erased<caching_level_type::full, H>());
}

BENCHMARK(BM_resolve_triangular_tree_erased_full<2>)
    ->Name("BM_resolve_function_request_erased_disk_cached_tri_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree_erased_full<4>)
    ->Name("BM_resolve_function_request_erased_disk_cached_tri_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree_erased_full<6>)
    ->Name("BM_resolve_function_request_erased_disk_cached_tri_tree H=6")
    ->Apply(thousand_loops);
