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
            rpclib_client = register_rpclib_client(make_inner_tests_config());
        }
    }
    else
    {
        throw std::invalid_argument(
            fmt::format("Unknown proxy name {}", proxy_name));
    }
}

template<caching_level_type caching_level, bool storing, typename Req>
void
BM_try_resolve_testing_request(
    benchmark::State& state, Req const& req, std::string const& proxy_name)
{
    inner_resources service;
    init_test_inner_service(service);
    bool remotely = proxy_name.size() > 0;
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
    benchmark::State& state, Req const& req, std::string const& proxy_name)
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

constexpr auto full = caching_level_type::full;
constexpr size_t tenK = 10'240;
constexpr size_t oneM = 1'048'576;
BENCHMARK(BM_resolve_make_some_blob<full, false, tenK, remoting::loopback>)
    ->Name("BM_resolve_make_some_blob_loopback_10K")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<full, false, tenK, remoting::copy>)
    ->Name("BM_resolve_make_some_blob_rpclib_copy_10K")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<full, false, tenK, remoting::shared>)
    ->Name("BM_resolve_make_some_blob_rpclib_shared_10K")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<full, false, oneM, remoting::copy>)
    ->Name("BM_resolve_make_some_blob_rpclib_copy_1M")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_make_some_blob<full, false, oneM, remoting::shared>)
    ->Name("BM_resolve_make_some_blob_rpclib_shared_1M")
    ->Apply(thousand_loops);
