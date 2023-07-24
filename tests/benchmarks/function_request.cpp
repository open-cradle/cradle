#include <benchmark/benchmark.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>

#include "../support/inner_service.h"
#include "benchmark_support.h"

using namespace cradle;

static auto add = [](int a, int b) { return a + b; };

static request_uuid
make_uuid()
{
    static int next_id{};
    return request_uuid{fmt::format("benchmark-{}", next_id++)};
}

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
    caching_request_resolution_context ctx{};
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
