#include <stdexcept>

#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>

#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/domain_factory.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/rpclib/client/proxy.h>

#include "../support/inner_service.h"
#include "benchmark_support.h"

using namespace cradle;
using namespace std;

template<caching_level_type caching_level, bool storing, typename Req>
void
BM_try_resolve_testing_request(
    benchmark::State& state, Req const& req, std::string const& proxy_name)
{
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    testing_request_context ctx{*resources, proxy_name};

    // Fill the appropriate cache if any
    auto init = [&]() -> cppcoro::task<void> {
        if constexpr (is_cached(caching_level))
        {
            benchmark::DoNotOptimize(co_await resolve_request(ctx, req));
            if constexpr (is_fully_cached(caching_level))
            {
                sync_wait_write_disk_cache(*resources);
            }
        }
        co_return;
    };
    cppcoro::sync_wait(init());

    int num_loops = static_cast<int>(state.range(0));
    for (auto _ : state)
    {
        auto loop = [&]() -> cppcoro::task<void> {
            for (int i = 0; i < num_loops; ++i)
            {
// Visual C++ 14.30 (2022) seems to be overly eager in reporting unused
// variables. (If the variable is only used in a path that is never taken for
// some values of constexpr variables, it complains.)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4189)
#endif
                constexpr bool need_empty_memory_cache
                    = is_fully_cached(caching_level) || storing;
                constexpr bool need_empty_disk_cache
                    = is_fully_cached(caching_level) && storing;
                if constexpr (need_empty_memory_cache || need_empty_disk_cache)
                {
                    // Some scenarios are problematic for some reason
                    // (huge CPU times, only one iteration).
                    // Not stopping and resuming timing gives some improvement.
                    constexpr bool problematic
                        = is_uncached(caching_level)
                          || (is_memory_cached(caching_level) && storing);
                    constexpr bool pause_timing = !problematic;
                    if constexpr (pause_timing)
                    {
                        state.PauseTiming();
                    }
                    if constexpr (need_empty_memory_cache)
                    {
                        resources->reset_memory_cache();
                    }
                    if constexpr (need_empty_disk_cache)
                    {
                        resources->clear_secondary_cache();
                    }
                    if constexpr (pause_timing)
                    {
                        state.ResumeTiming();
                    }
                }
                benchmark::DoNotOptimize(co_await resolve_request(ctx, req));
#ifdef _MSC_VER
#pragma warning(pop)
#endif
            }
            co_return;
        };
        cppcoro::sync_wait(loop());
    }
}

template<caching_level_type caching_level, bool storing, typename Req>
void
BM_resolve_testing_request(
    benchmark::State& state, Req const& req, std::string const& proxy_name)
{
    try
    {
        BM_try_resolve_testing_request<caching_level, storing, Req>(
            state, req, proxy_name);
    }
    catch (std::exception& e)
    {
        handle_benchmark_exception(state, e.what());
    }
    catch (...)
    {
        handle_benchmark_exception(state, "Caught unknown exception");
    }
}

enum class remoting
{
    none,
    loopback,
    copy,
    shared
};

template<
    caching_level_type caching_level,
    bool storing,
    size_t size = 10240,
    remoting remote = remoting::none>
void
BM_resolve_make_some_blob(benchmark::State& state)
{
    std::string proxy_name;
    bool shared = false;
    switch (remote)
    {
        case remoting::none:
            shared = false;
            break;
        case remoting::loopback:
            proxy_name = "loopback";
            shared = false;
            break;
        case remoting::copy:
            proxy_name = "rpclib";
            shared = false;
            break;
        case remoting::shared:
            proxy_name = "rpclib";
            shared = true;
            break;
    }
    auto req{rq_make_some_blob<caching_level>(size, shared)};
    BM_resolve_testing_request<caching_level, storing>(state, req, proxy_name);
}

constexpr auto full = caching_level_type::full;
constexpr size_t tenK = 10'240;
constexpr size_t oneM = 1'048'576;

BENCHMARK(BM_resolve_make_some_blob<caching_level_type::none, false, tenK>)
    ->Name("BM_resolve_make_some_blob_uncached_10K")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::none, false, oneM>)
    ->Name("BM_resolve_make_some_blob_uncached_1M")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::memory, true, tenK>)
    ->Name("BM_resolve_make_some_blob_store_to_mem_cache_10K")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::memory, true, oneM>)
    ->Name("BM_resolve_make_some_blob_store_to_mem_cache_1M")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::memory, false, tenK>)
    ->Name("BM_resolve_make_some_blob_mem_cached_10K")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::memory, false, oneM>)
    ->Name("BM_resolve_make_some_blob_mem_cached_1M")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::full, false, tenK>)
    ->Name("BM_resolve_make_some_blob_disk_cached_10K")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::full, false, oneM>)
    ->Name("BM_resolve_make_some_blob_disk_cached_1M")
    ->Apply(thousand_loops);
#if 0
/*
Current/previous problems with benchmarking disk caching:
(a) The disk cache wasn't be cleared between runs; this has been fixed.
(b) A race condition: issue #231.
*/
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::full, true>)
    ->Name("BM_resolve_make_some_blob_store_to_disk_cache")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::full, false>)
    ->Name("BM_resolve_make_some_blob_load_from_disk_cache")
    ->Apply(thousand_loops);
#endif

BENCHMARK(BM_resolve_make_some_blob<full, false, tenK, remoting::loopback>)
    ->Name("BM_resolve_make_some_blob_loopback_10K")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<full, false, oneM, remoting::loopback>)
    ->Name("BM_resolve_make_some_blob_loopback_1M")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<full, false, tenK, remoting::copy>)
    ->Name("BM_resolve_make_some_blob_rpclib_copy_10K")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<full, false, oneM, remoting::copy>)
    ->Name("BM_resolve_make_some_blob_rpclib_copy_1M")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<full, false, tenK, remoting::shared>)
    ->Name("BM_resolve_make_some_blob_rpclib_shared_10K")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<full, false, oneM, remoting::shared>)
    ->Name("BM_resolve_make_some_blob_rpclib_shared_1M")
    ->Apply(thousand_loops);
