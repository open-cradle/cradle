#include <typeindex>
#include <typeinfo>

#include <benchmark/benchmark.h>

#include "id_support.h"

using namespace cradle;

/*
 * Benchmark various ways of comparing typeinfo values.
 *
 * Conclusions:
 * - Comparing names (i.e., comparing pointers to C-style strings) tends to
 *   be faster but there is no formal guarantee that this is correct.
 * - Incorrect behaviour mostly seems to be possible in dlopen scenarios.
 * - Comparing typeinfo's or typeindex's is equally fast.
 * - GNU's default typeinfo comparison is a conservative strcmp(), but
 *   depending on configuration, it may use the optimized pointer comparison.
 * - Comparison is significantly slower with VS2019 compared to GCC or Clang.
 */

template<typename Id0, typename Id1>
static void
BM_compare_type_info(benchmark::State& state, Id0 const& id0, Id1 const& id1)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(typeid(id0) == typeid(id1));
    }
}

template<typename Id0, typename Id1>
static void
BM_compare_type_name(benchmark::State& state, Id0 const& id0, Id1 const& id1)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(typeid(id0).name() == typeid(id1).name());
    }
}

template<typename Id0, typename Id1>
static void
BM_compare_type_index(benchmark::State& state, Id0 const& id0, Id1 const& id1)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(
            std::type_index(typeid(id0)) == std::type_index(typeid(id1)));
    }
}

BENCHMARK_CAPTURE(
    BM_compare_type_info, int_int, make_id<int>(0), make_id<int>(0));
BENCHMARK_CAPTURE(
    BM_compare_type_name, int_int, make_id<int>(0), make_id<int>(0));
BENCHMARK_CAPTURE(
    BM_compare_type_index, int_int, make_id<int>(0), make_id<int>(0));

BENCHMARK_CAPTURE(
    BM_compare_type_info,
    my_struct_S0_S1,
    get_my_struct_id0(),
    get_my_struct_id1());
BENCHMARK_CAPTURE(
    BM_compare_type_name,
    my_struct_S0_S1,
    get_my_struct_id0(),
    get_my_struct_id1());
BENCHMARK_CAPTURE(
    BM_compare_type_index,
    my_struct_S0_S1,
    get_my_struct_id0(),
    get_my_struct_id1());
