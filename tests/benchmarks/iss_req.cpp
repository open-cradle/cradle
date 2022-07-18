#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

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

template<caching_level_type caching_level>
static void
BM_create_thinknode_request(benchmark::State& state)
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

static void
BM_create_thinknode_request_uncached(benchmark::State& state)
{
    BM_create_thinknode_request<caching_level_type::none>(state);
}

BENCHMARK(BM_create_thinknode_request_uncached);

static void
BM_create_thinknode_request_memory_cached(benchmark::State& state)
{
    BM_create_thinknode_request<caching_level_type::memory>(state);
}

BENCHMARK(BM_create_thinknode_request_memory_cached);

static void
set_mock_script(mock_http_session& mock_http, int num_loops)
{
    mock_http_script script;
    mock_http_exchange exchange{
        make_http_request(
            http_request_method::POST,
            "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
            {{"Authorization", "Bearer xyz"},
             {"Accept", "application/json"},
             {"Content-Type", "application/octet-stream"}},
            make_blob("payload")),
        make_http_200_response("{ \"id\": \"def\" }")};
    for (int i = 0; i < num_loops; ++i)
    {
        script.push_back(exchange);
    }
    mock_http.set_script(script);
}

template<Request Req>
cppcoro::task<string>
resolve_n_requests(int n, thinknode_request_context& ctx, Req const& req)
{
    string result;
    for (int i = 0; i < n; ++i)
    {
        result += co_await resolve_request(ctx, req);
    }
    co_return result;
}

template<caching_level_type caching_level>
void
BM_resolve_thinknode_request(benchmark::State& state)
{
    service_core service;
    init_test_service(service);
    auto& mock_http = enable_http_mocking(service);
    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    thinknode_request_context ctx{service, session, nullptr};
    string context_id{"123"};
    auto schema{
        make_thinknode_type_info_with_string_type(thinknode_string_type())};
    auto object_data{make_blob("payload")};
    auto req{rq_post_iss_object<caching_level>(
        session.api_url, context_id, schema, object_data)};

    // Fill the memory cache if used
    if constexpr (caching_level != caching_level_type::none)
    {
        set_mock_script(mock_http, 1);
        cppcoro::sync_wait(resolve_n_requests(1, ctx, req));
    }

    for (auto _ : state)
    {
        int num_loops = static_cast<int>(state.range(0));
        if constexpr (caching_level == caching_level_type::none)
        {
            state.PauseTiming();
            set_mock_script(mock_http, state.range(0));
            state.ResumeTiming();
        }
        cppcoro::sync_wait(resolve_n_requests(num_loops, ctx, req));
    }
}

#if 0
// This benchmark should measure the resolve_request overhead time but
// the request itself, and especially the mock HTTP overhead, take much longer.
// It would be useful to compare against resolving a memory-cached request
// that stores the result in the cache.
static void
BM_resolve_thinknode_request_uncached(benchmark::State& state)
{
    BM_resolve_thinknode_request<caching_level_type::none>(state);
}

BENCHMARK(BM_resolve_thinknode_request_uncached)->Arg(1)->Arg(10)->Arg(100)->Arg(1000);
#endif

static void
BM_resolve_thinknode_request_memory_cached(benchmark::State& state)
{
    BM_resolve_thinknode_request<caching_level_type::memory>(state);
}

// 1000 inner loops bring the sync_wait() overhead down to amortized zero
BENCHMARK(BM_resolve_thinknode_request_memory_cached)->Arg(1000);
