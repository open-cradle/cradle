#include <benchmark/benchmark.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/service/resources.h>

#include "../support/inner_service.h"
#include "benchmark_support.h"

using namespace cradle;

template<caching_level_type level>
struct request_resolution_context_struct
{
    using type = caching_request_resolution_context;
};

template<>
struct request_resolution_context_struct<caching_level_type::none>
{
    using type = non_caching_request_resolution_context;
};

template<caching_level_type level>
using request_resolution_context =
    typename request_resolution_context_struct<level>::type;

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
        return rq_function(props, add, 2, 1);
    }
    else
    {
        return rq_function(
            props, add, create_thin_tree_erased<level, H - 1>(), 1);
    }
}

template<caching_level_type level, int H, bool recursive_vbc = false>
    requires(!is_fully_cached(level))
auto create_triangular_tree_erased()
{
    request_props<level> props{make_uuid()};
    if constexpr (H == 1)
    {
        return rq_function(props, add, 2, 1);
    }
    else
    {
        constexpr auto next_level
            = recursive_vbc ? level : to_composition_based(level);
        return rq_function(
            props,
            add,
            create_triangular_tree_erased<next_level, H - 1, recursive_vbc>(),
            create_triangular_tree_erased<next_level, H - 1, recursive_vbc>());
    }
}

template<caching_level_type level, int H>
    requires(is_fully_cached(level))
auto create_triangular_tree_erased()
{
    request_props<level, request_function_t::plain, false> props{make_uuid()};
    if constexpr (H == 1)
    {
        return rq_function(props, add, 2, 1);
    }
    else
    {
        return rq_function(
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
    using props_type = request_props<level, request_function_t::plain, true>;
    if constexpr (H == 1)
    {
        props_type props{make_uuid(), "add 2+1"};
        return rq_function(props, add, 2, 1);
    }
    else
    {
        std::string title{fmt::format("add H{}", H)};
        props_type props{make_uuid(), title};
        return rq_function(
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
    ->Name("BM_create_function_request_uncached_thin_tree H=2");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::none, 4>)
    ->Name("BM_create_function_request_uncached_thin_tree H=4");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::none, 16>)
    ->Name("BM_create_function_request_uncached_thin_tree H=16");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::none, 64>)
    ->Name("BM_create_function_request_uncached_thin_tree H=64");

BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::none, 2>)
    ->Name("BM_create_function_request_uncached_tri_tree H=2");
BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::none, 4>)
    ->Name("BM_create_function_request_uncached_tri_tree H=4");
BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::none, 6>)
    ->Name("BM_create_function_request_uncached_tri_tree H=6");

BENCHMARK(BM_create_thin_tree_erased<caching_level_type::memory, 2>)
    ->Name("BM_create_function_request_cached_thin_tree H=2");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::memory, 4>)
    ->Name("BM_create_function_request_cached_thin_tree H=4");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::memory, 16>)
    ->Name("BM_create_function_request_cached_thin_tree H=16");
BENCHMARK(BM_create_thin_tree_erased<caching_level_type::memory, 64>)
    ->Name("BM_create_function_request_cached_thin_tree H=64");

BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::memory, 2>)
    ->Name("BM_create_function_request_cached_tri_tree H=2");
BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::memory, 4>)
    ->Name("BM_create_function_request_cached_tri_tree H=4");
BENCHMARK(BM_create_triangular_tree_erased<caching_level_type::memory, 6>)
    ->Name("BM_create_function_request_cached_tri_tree H=6");

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
    ->Name("BM_create_function_request_cached_intrsp_tri_tree H=4");
BENCHMARK(BM_create_tri_tree_erased_intrsp<caching_level_type::memory, 6>)
    ->Name("BM_create_function_request_cached_intrsp_tri_tree H=6");

template<caching_level_type level, int H>
void
BM_resolve_thin_tree_erased(benchmark::State& state)
{
    auto resources{make_inner_test_resources()};
    request_resolution_context<level> ctx{*resources};
    BM_resolve_request(state, ctx, create_thin_tree_erased<level, H>());
}

BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::none, 2>)
    ->Name("BM_resolve_function_request_uncached_thin_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::none, 4>)
    ->Name("BM_resolve_function_request_uncached_thin_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::none, 16>)
    ->Name("BM_resolve_function_request_uncached_thin_tree H=16")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::none, 64>)
    ->Name("BM_resolve_function_request_uncached_thin_tree H=64")
    ->Apply(thousand_loops);

template<caching_level_type level, int H, bool recursive_vbc = false>
void
BM_resolve_tri_tree_erased(benchmark::State& state)
{
    auto resources{make_inner_test_resources()};
    request_resolution_context<level> ctx{*resources};
    BM_resolve_request(
        state, ctx, create_triangular_tree_erased<level, H, recursive_vbc>());
}

BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::none, 2>)
    ->Name("BM_resolve_function_request_uncached_tri_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::none, 4>)
    ->Name("BM_resolve_function_request_uncached_tri_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::none, 6>)
    ->Name("BM_resolve_function_request_uncached_tri_tree H=6")
    ->Apply(thousand_loops);

BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::memory, 2>)
    ->Name("BM_resolve_function_request_mem_cached_thin_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::memory, 4>)
    ->Name("BM_resolve_function_request_mem_cached_thin_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::memory, 16>)
    ->Name("BM_resolve_function_request_mem_cached_thin_tree H=16")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_thin_tree_erased<caching_level_type::memory, 64>)
    ->Name("BM_resolve_function_request_mem_cached_thin_tree H=64")
    ->Apply(thousand_loops);

// The VBC-top benchmarks apply value-based caching to the topmost root
// request only, which probably is how it should be used in practice.
// The VBC-all benchmarks apply value-based caching recursively to all
// requests, basically defeating the caching mechanism.
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory, 2>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=2 CBC")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory_vb, 2, false>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=2 VBC-top")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory_vb, 2, true>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=2 VBC-all")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory, 4>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=4 CBC")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory_vb, 4, false>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=4 VBC-top")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory_vb, 4, true>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=4 VBC-all")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory, 6>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=6 CBC")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory_vb, 6, false>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=6 VBC-top")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_tri_tree_erased<caching_level_type::memory_vb, 6, true>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree H=6 VBC-all")
    ->Apply(thousand_loops);

template<caching_level_type level, int H>
void
BM_resolve_tri_tree_erased_unk_ctx(benchmark::State& state)
{
    auto resources{make_inner_test_resources()};
    request_resolution_context<level> ctx{*resources};
    // Don't tell the framework what the actual context type is,
    // making some optimizations impossible.
    context_intf& unk_ctx{ctx};
    BM_resolve_request(
        state, unk_ctx, create_triangular_tree_erased<level, H>());
}

BENCHMARK(BM_resolve_tri_tree_erased_unk_ctx<caching_level_type::memory, 6>)
    ->Name("BM_resolve_function_request_mem_cached_tri_tree unk ctx H=6")
    ->Apply(thousand_loops);

template<int H>
void
BM_resolve_triangular_tree_erased_full(benchmark::State& state)
{
    auto resources{make_inner_test_resources()};
    caching_request_resolution_context ctx{*resources};
    spdlog::set_level(spdlog::level::warn);
    BM_resolve_request(
        state,
        ctx,
        create_triangular_tree_erased<caching_level_type::full, H>());
}

BENCHMARK(BM_resolve_triangular_tree_erased_full<2>)
    ->Name("BM_resolve_function_request_disk_cached_tri_tree H=2")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree_erased_full<4>)
    ->Name("BM_resolve_function_request_disk_cached_tri_tree H=4")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_triangular_tree_erased_full<6>)
    ->Name("BM_resolve_function_request_disk_cached_tri_tree H=6")
    ->Apply(thousand_loops);
