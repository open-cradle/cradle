#include <stdexcept>
#include <thread>

#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/io/mock_http.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/thinknode/caching.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/typing/encodings/msgpack.h>
#include <cradle/typing/utilities/testing.h>

#include "../support/inner_service.h"
#include "../support/thinknode.h"
#include "benchmark_support.h"

using namespace cradle;
using namespace std;

template<caching_level_type caching_level>
static void
BM_create_post_iss_request(benchmark::State& state)
{
    string context_id{"123"};
    auto schema{
        make_thinknode_type_info_with_string_type(thinknode_string_type())};
    auto object_data{make_blob("payload")};

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(rq_post_iss_object<caching_level>(
            context_id, schema, object_data));
    }
}

BENCHMARK(BM_create_post_iss_request<caching_level_type::none>)
    ->Name("BM_create_post_iss_request_uncached");
BENCHMARK(BM_create_post_iss_request<caching_level_type::memory>)
    ->Name("BM_create_post_iss_request_memory_cached");
BENCHMARK(BM_create_post_iss_request<caching_level_type::full>)
    ->Name("BM_create_post_iss_request_fully_cached");

static char const s_loopback[] = "loopback";
static char const s_rpclib[] = "rpclib";

template<caching_level_type caching_level, bool storing, typename Req>
void
BM_try_resolve_thinknode_request(
    benchmark::State& state,
    Req const& req,
    http_response const& response,
    char const* proxy_name_ptr = nullptr)
{
    std::string proxy_name;
    if (proxy_name_ptr)
    {
        proxy_name = proxy_name_ptr;
    }
    thinknode_test_scope scope{proxy_name};
    auto& resources{scope.get_resources()};
    if (auto proxy = scope.get_proxy())
    {
        auto const& body{response.body};
        std::string s{reinterpret_cast<char const*>(body.data()), body.size()};
        proxy->mock_http(s);
    }
    else
    {
        auto& mock_http = resources.enable_http_mocking();
        mock_http.set_canned_response(response);
    }
    auto ctx{scope.make_context()};

    // Fill the appropriate cache if any
    auto init = [&]() -> cppcoro::task<void> {
        if constexpr (is_cached(caching_level))
        {
            benchmark::DoNotOptimize(co_await resolve_request(ctx, req));
            if constexpr (is_fully_cached(caching_level))
            {
                sync_wait_write_disk_cache(resources);
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
                        resources.reset_memory_cache();
                    }
                    if constexpr (need_empty_disk_cache)
                    {
                        resources.clear_secondary_cache();
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
BM_resolve_thinknode_request(
    benchmark::State& state,
    Req const& req,
    http_response const& response,
    char const* proxy_name = nullptr)
{
    try
    {
        BM_try_resolve_thinknode_request<caching_level, storing, Req>(
            state, req, response, proxy_name);
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

template<
    caching_level_type caching_level,
    bool storing = false,
    char const* proxy_name = nullptr>
void
BM_resolve_post_iss_request(benchmark::State& state)
{
    string context_id{"123"};
    auto schema{
        make_thinknode_type_info_with_string_type(thinknode_string_type())};
    auto object_data{make_blob("payload")};
    auto req{
        rq_post_iss_object<caching_level>(context_id, schema, object_data)};
    auto response{make_http_200_response("{ \"id\": \"def\" }")};
    BM_resolve_thinknode_request<caching_level, storing>(
        state, req, response, proxy_name);
}

BENCHMARK(BM_resolve_post_iss_request<caching_level_type::none>)
    ->Name("BM_resolve_post_iss_request_uncached")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_post_iss_request<caching_level_type::memory, true>)
    ->Name("BM_resolve_post_iss_request_store_to_mem_cache")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_post_iss_request<caching_level_type::memory, false>)
    ->Name("BM_resolve_post_iss_request_load_from_mem_cache")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_post_iss_request<caching_level_type::full, true>)
    ->Name("BM_resolve_post_iss_request_store_to_disk_cache")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_post_iss_request<caching_level_type::full, false>)
    ->Name("BM_resolve_post_iss_request_load_from_disk_cache")
    ->Apply(thousand_loops);
BENCHMARK(
    BM_resolve_post_iss_request<caching_level_type::full, false, s_loopback>)
    ->Name("BM_resolve_post_iss_request_loopback")
    ->Apply(thousand_loops);
BENCHMARK(
    BM_resolve_post_iss_request<caching_level_type::full, false, s_rpclib>)
    ->Name("BM_resolve_post_iss_request_rpclib")
    ->Apply(thousand_loops);

template<
    caching_level_type caching_level,
    bool storing = false,
    char const* proxy_name = nullptr>
void
BM_resolve_retrieve_immutable_request(benchmark::State& state)
{
    string context_id{"123"};
    string immutable_id{"abc"};
    auto req{
        rq_retrieve_immutable_object<caching_level>(context_id, immutable_id)};
    auto response{make_http_200_response("payload")};
    BM_resolve_thinknode_request<caching_level, storing>(
        state, req, response, proxy_name);
}

BENCHMARK(
    BM_resolve_retrieve_immutable_request<caching_level_type::none, false>)
    ->Name("BM_resolve_retrieve_immutable_request_uncached")
    ->Apply(thousand_loops);
BENCHMARK(
    BM_resolve_retrieve_immutable_request<caching_level_type::memory, true>)
    ->Name("BM_resolve_retrieve_immutable_request_store_to_mem_cache")
    ->Apply(thousand_loops);
BENCHMARK(
    BM_resolve_retrieve_immutable_request<caching_level_type::memory, false>)
    ->Name("BM_resolve_retrieve_immutable_request_load_from_mem_cache")
    ->Apply(thousand_loops);
BENCHMARK(
    BM_resolve_retrieve_immutable_request<caching_level_type::full, true>)
    ->Name("BM_resolve_retrieve_immutable_request_store_to_disk_cache")
    ->Apply(thousand_loops);
BENCHMARK(
    BM_resolve_retrieve_immutable_request<caching_level_type::full, false>)
    ->Name("BM_resolve_retrieve_immutable_request_load_from_disk_cache")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_retrieve_immutable_request<
              caching_level_type::full,
              false,
              s_loopback>)
    ->Name("BM_resolve_retrieve_immutable_request_loopback")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_retrieve_immutable_request<
              caching_level_type::full,
              false,
              s_rpclib>)
    ->Name("BM_resolve_retrieve_immutable_request_rpclib")
    ->Apply(thousand_loops);
