#include <stdexcept>

#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>

#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/registry.h>

#include "../support/inner_service.h"
#include "benchmark_support.h"

using namespace cradle;
using namespace std;

static void
register_remote_services(std::string const& proxy_name)
{
    static bool registered_resolvers = false;
    if (!registered_resolvers)
    {
        register_testing_seri_resolvers();
        registered_resolvers = true;
    }
    if (proxy_name == "loopback")
    {
        static bool registered_loopback;
        if (!registered_loopback)
        {
            register_loopback_service();
            registered_loopback = true;
        }
    }
    else if (proxy_name == "rpclib")
    {
        // TODO no static here, add func to get previously registered client
        static std::shared_ptr<rpclib_client> rpclib_client;
        if (!rpclib_client)
        {
            // TODO pass non-default config?
            rpclib_client = register_rpclib_client(service_config());
        }
    }
    else
    {
        throw std::invalid_argument(
            fmt::format("Unknown proxy name {}", proxy_name));
    }
}

static char const s_loopback[] = "loopback";
static char const s_rpclib[] = "rpclib";

template<caching_level_type caching_level, bool storing, typename Req>
void
BM_try_resolve_testing_request(
    benchmark::State& state, Req const& req, char const* proxy_name = nullptr)
{
    inner_resources service;
    init_test_inner_service(service);
    bool remotely = proxy_name != nullptr;
    testing_request_context ctx{service, nullptr, remotely};
    if (remotely)
    {
        register_remote_services(proxy_name);
        ctx.proxy_name(proxy_name);
    }

    // Fill the appropriate cache if any
    auto init = [&]() -> cppcoro::task<void> {
        if constexpr (caching_level != caching_level_type::none)
        {
            benchmark::DoNotOptimize(co_await resolve_request(ctx, req));
            if constexpr (caching_level == caching_level_type::full)
            {
                sync_wait_write_disk_cache(service);
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
                constexpr bool need_empty_memory_cache
                    = caching_level == caching_level_type::full || storing;
                constexpr bool need_empty_disk_cache
                    = caching_level == caching_level_type::full && storing;
                if constexpr (need_empty_memory_cache || need_empty_disk_cache)
                {
                    // Some scenarios are problematic for some reason
                    // (huge CPU times, only one iteration).
                    // Not stopping and resuming timing gives some improvement.
                    constexpr bool problematic
                        = caching_level == caching_level_type::none
                          || (caching_level == caching_level_type::memory
                              && storing);
                    constexpr bool pause_timing = !problematic;
                    if constexpr (pause_timing)
                    {
                        state.PauseTiming();
                    }
                    if constexpr (need_empty_memory_cache)
                    {
                        service.inner_reset_memory_cache();
                    }
                    if constexpr (need_empty_disk_cache)
                    {
                        inner_reset_disk_cache(service);
                    }
                    if constexpr (pause_timing)
                    {
                        state.ResumeTiming();
                    }
                }
                benchmark::DoNotOptimize(co_await resolve_request(ctx, req));
            }
            co_return;
        };
        cppcoro::sync_wait(loop());
    }
}

template<caching_level_type caching_level, bool storing, typename Req>
void
BM_resolve_testing_request(
    benchmark::State& state, Req const& req, char const* proxy_name = nullptr)
{
    try
    {
        BM_try_resolve_testing_request<caching_level, storing, Req>(
            state, req, proxy_name);
    }
    catch (std::exception& e)
    {
        state.SkipWithError(e.what());
    }
    catch (...)
    {
        state.SkipWithError("Caught unknown exception");
    }
}

template<
    caching_level_type caching_level,
    bool storing = false,
    char const* proxy_name = nullptr>
void
BM_resolve_make_some_blob(benchmark::State& state)
{
    auto req{rq_make_some_blob<caching_level>(10000)};
    BM_resolve_testing_request<caching_level, storing>(state, req, proxy_name);
}

BENCHMARK(BM_resolve_make_some_blob<caching_level_type::none, false>)
    ->Name("BM_resolve_make_some_blob_uncached")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::memory, true>)
    ->Name("BM_resolve_make_some_blob_store_to_mem_cache")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::memory, false>)
    ->Name("BM_resolve_make_some_blob_load_from_mem_cache")
    ->Apply(thousand_loops);
#if 0
/*
Current problems with benchmarking disk caching:
(a) The disk cache should be cleared between runs, but inner_reset_disk_cache() does not do that.
(b) A race condition: issue #231.
*/
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::full, true>)
    ->Name("BM_resolve_make_some_blob_store_to_disk_cache")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::full, false>)
    ->Name("BM_resolve_make_some_blob_load_from_disk_cache")
    ->Apply(thousand_loops);
#endif
BENCHMARK(
    BM_resolve_make_some_blob<caching_level_type::full, false, s_loopback>)
    ->Name("BM_resolve_make_some_blob_loopback")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<caching_level_type::full, false, s_rpclib>)
    ->Name("BM_resolve_make_some_blob_rpclib")
    ->Apply(thousand_loops);
