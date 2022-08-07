#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/requests/value.h>
#include <cradle/inner/service/core.h>
#include <cradle/inner/service/request.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/typing/encodings/msgpack.h>
#include <cradle/typing/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

#include "../inner/support/core.h"
#include "support.h"

using namespace cradle;
using namespace std;

template<caching_level_type caching_level>
static void
BM_create_post_iss_request(benchmark::State& state)
{
    string api_url{"https://mgh.thinknode.io/api/v1.0"};
    string context_id{"123"};
    auto schema{
        make_thinknode_type_info_with_string_type(thinknode_string_type())};
    auto object_data{make_blob("payload")};

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(rq_post_iss_object<caching_level>(
            api_url, context_id, schema, object_data));
    }
}

BENCHMARK(BM_create_post_iss_request<caching_level_type::none>)
    ->Name("BM_create_post_iss_request_uncached");
BENCHMARK(BM_create_post_iss_request<caching_level_type::memory>)
    ->Name("BM_create_post_iss_request_memory_cached");
BENCHMARK(BM_create_post_iss_request<caching_level_type::full>)
    ->Name("BM_create_post_iss_request_fully_cached");

template<caching_level_type caching_level>
static void
BM_create_post_iss_request_erased(benchmark::State& state)
{
    string api_url{"https://mgh.thinknode.io/api/v1.0"};
    string context_id{"123"};
    auto schema{
        make_thinknode_type_info_with_string_type(thinknode_string_type())};
    auto object_data{make_blob("payload")};

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(rq_post_iss_object_erased<caching_level>(
            api_url, context_id, schema, object_data));
    }
}

BENCHMARK(BM_create_post_iss_request_erased<caching_level_type::none>)
    ->Name("BM_create_post_iss_request_erased_uncached");
BENCHMARK(BM_create_post_iss_request_erased<caching_level_type::memory>)
    ->Name("BM_create_post_iss_request_erased_memory_cached");
BENCHMARK(BM_create_post_iss_request_erased<caching_level_type::full>)
    ->Name("BM_create_post_iss_request_erased_fully_cached");

static void
set_mock_script(
    mock_http_session& mock_http,
    mock_http_exchange const& exchange,
    int num_loops)
{
    mock_http_script script;
    for (int i = 0; i < num_loops; ++i)
    {
        script.push_back(exchange);
    }
    mock_http.set_script(script);
}

template<caching_level_type caching_level, bool storing, typename Req>
void
BM_resolve_thinknode_request(
    benchmark::State& state,
    Req const& req,
    string const& api_url,
    mock_http_exchange const& exchange)
{
    service_core service;
    init_test_service(service);
    auto& mock_http = enable_http_mocking(service);
    thinknode_session session;
    session.api_url = api_url;
    session.access_token = "xyz";
    thinknode_request_context ctx{service, session, nullptr};

    // Suppress output about disk cache hits
    if constexpr (caching_level == caching_level_type::full)
    {
        spdlog::set_level(spdlog::level::warn);
    }

    // Fill the appropriate cache if any
    auto init = [&]() -> cppcoro::task<void> {
        if constexpr (caching_level != caching_level_type::none)
        {
            set_mock_script(mock_http, exchange, 1);
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
                constexpr bool need_mock_script
                    = caching_level == caching_level_type::none || storing;
                constexpr bool need_empty_memory_cache
                    = caching_level == caching_level_type::full || storing;
                constexpr bool need_empty_disk_cache
                    = caching_level == caching_level_type::full && storing;
                if constexpr (
                    need_mock_script || need_empty_memory_cache
                    || need_empty_disk_cache)
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
                    if constexpr (need_mock_script)
                    {
                        set_mock_script(mock_http, exchange, 1);
                    }
                    if constexpr (need_empty_memory_cache)
                    {
                        service.inner_reset_memory_cache();
                    }
                    if constexpr (need_empty_disk_cache)
                    {
                        service.inner_reset_disk_cache();
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

template<caching_level_type caching_level, bool storing>
void
BM_resolve_post_iss_request_impl(auto& create_request, benchmark::State& state)
{
    string api_url{"https://mgh.thinknode.io/api/v1.0"};
    string context_id{"123"};
    auto schema{
        make_thinknode_type_info_with_string_type(thinknode_string_type())};
    auto object_data{make_blob("payload")};
    auto req{create_request(api_url, context_id, schema, object_data)};
    mock_http_exchange exchange{
        make_http_request(
            http_request_method::POST,
            "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
            {{"Authorization", "Bearer xyz"},
             {"Accept", "application/json"},
             {"Content-Type", "application/octet-stream"}},
            make_blob("payload")),
        make_http_200_response("{ \"id\": \"def\" }")};

    BM_resolve_thinknode_request<caching_level, storing>(
        state, req, api_url, exchange);
}

template<caching_level_type caching_level, bool storing = false>
void
BM_resolve_post_iss_request(benchmark::State& state)
{
    auto create_request = [](string api_url,
                             string context_id,
                             thinknode_type_info schema,
                             blob object_data) {
        return rq_post_iss_object<caching_level>(
            api_url, context_id, schema, object_data);
    };
    BM_resolve_post_iss_request_impl<caching_level, storing>(
        create_request, state);
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

template<caching_level_type caching_level, bool storing = false>
void
BM_resolve_post_iss_request_erased(benchmark::State& state)
{
    auto create_request = [](string api_url,
                             string context_id,
                             thinknode_type_info schema,
                             blob object_data) {
        return rq_post_iss_object_erased<caching_level>(
            api_url, context_id, schema, object_data);
    };
    BM_resolve_post_iss_request_impl<caching_level, storing>(
        create_request, state);
}

BENCHMARK(BM_resolve_post_iss_request_erased<caching_level_type::none>)
    ->Name("BM_resolve_post_iss_request_erased_uncached")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_post_iss_request_erased<caching_level_type::memory, true>)
    ->Name("BM_resolve_post_iss_request_erased_store_to_mem_cache")
    ->Apply(thousand_loops);
BENCHMARK(
    BM_resolve_post_iss_request_erased<caching_level_type::memory, false>)
    ->Name("BM_resolve_post_iss_request_erased_load_from_mem_cache")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_post_iss_request_erased<caching_level_type::full, true>)
    ->Name("BM_resolve_post_iss_request_erased_store_to_disk_cache")
    ->Apply(thousand_loops);
BENCHMARK(BM_resolve_post_iss_request_erased<caching_level_type::full, false>)
    ->Name("BM_resolve_post_iss_request_erased_load_from_disk_cache")
    ->Apply(thousand_loops);

template<caching_level_type caching_level, bool storing>
void
BM_resolve_retrieve_immutable_request(benchmark::State& state)
{
    string api_url{"https://mgh.thinknode.io/api/v1.0"};
    string context_id{"123"};
    string immutable_id{"abc"};
    auto req{rq_retrieve_immutable_object<caching_level>(
        api_url, context_id, immutable_id)};
    mock_http_exchange exchange{
        make_get_request(
            "https://mgh.thinknode.io/api/v1.0/iss/immutable/"
            "abc?context=123",
            {{"Authorization", "Bearer xyz"},
             {"Accept", "application/octet-stream"}}),
        make_http_200_response("payload")};
    BM_resolve_thinknode_request<caching_level, storing>(
        state, req, api_url, exchange);
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
