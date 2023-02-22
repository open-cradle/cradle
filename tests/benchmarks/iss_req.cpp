#include <stdexcept>
#include <thread>

#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/service/request.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/serialization/disk_cache/preferred/cereal/cereal.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/registry.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/thinknode/seri_catalog.h>
#include <cradle/typing/encodings/msgpack.h>
#include <cradle/typing/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

#include "../support/inner_service.h"
#include "../support/outer_service.h"
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

static void
register_remote_services(
    std::string const& proxy_name, http_response const& response)
{
    static bool registered_resolvers = false;
    if (!registered_resolvers)
    {
        register_thinknode_seri_resolvers();
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
            rpclib_client = register_rpclib_client(make_outer_tests_config());
        }
        // TODO should body be blob or string?
        auto const& body{response.body};
        std::string s{reinterpret_cast<char const*>(body.data()), body.size()};
        rpclib_client->mock_http(s);
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
BM_try_resolve_thinknode_request(
    benchmark::State& state,
    Req const& req,
    string const& api_url,
    http_response const& response,
    char const* proxy_name = nullptr)
{
    service_core service;
    init_test_service(service);
    auto& mock_http = enable_http_mocking(service, true);
    mock_http.set_canned_response(response);
    thinknode_session session;
    session.api_url = api_url;
    session.access_token = "xyz";
    bool remotely = proxy_name != nullptr;
    thinknode_request_context ctx{service, session, nullptr, remotely};
    if (remotely)
    {
        register_remote_services(proxy_name, response);
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
BM_resolve_thinknode_request(
    benchmark::State& state,
    Req const& req,
    string const& api_url,
    http_response const& response,
    char const* proxy_name = nullptr)
{
    try
    {
        BM_try_resolve_thinknode_request<caching_level, storing, Req>(
            state, req, api_url, response, proxy_name);
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
BM_resolve_post_iss_request(benchmark::State& state)
{
    string api_url{"https://mgh.thinknode.io/api/v1.0"};
    string context_id{"123"};
    auto schema{
        make_thinknode_type_info_with_string_type(thinknode_string_type())};
    auto object_data{make_blob("payload")};
    auto req{
        rq_post_iss_object<caching_level>(context_id, schema, object_data)};
    auto response{make_http_200_response("{ \"id\": \"def\" }")};
    BM_resolve_thinknode_request<caching_level, storing>(
        state, req, api_url, response, proxy_name);
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
    string api_url{"https://mgh.thinknode.io/api/v1.0"};
    string context_id{"123"};
    string immutable_id{"abc"};
    auto req{
        rq_retrieve_immutable_object<caching_level>(context_id, immutable_id)};
    auto response{make_http_200_response("payload")};
    BM_resolve_thinknode_request<caching_level, storing>(
        state, req, api_url, response, proxy_name);
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
