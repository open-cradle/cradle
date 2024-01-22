#include <string>

#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/hash.h>
#include <cradle/plugins/domain/testing/requests.h>

#include "../support/common.h"
#include "../support/inner_service.h"

using namespace cradle;

auto
make_my_blob()
{
    std::size_t const size{1000};
    std::string proxy_name{};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    testing_request_context ctx{*resources, nullptr, proxy_name};
    return cppcoro::sync_wait(make_some_blob(ctx, size, false));
}

void
BM_BoostHash(benchmark::State& state)
{
    auto the_blob = make_my_blob();
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(invoke_hash(the_blob));
    }
}

// Finding an existing blob in the memory cache means calculating a Boost
// hash over the blob, plus comparing the (identical) blobs for equality.
void
BM_CompareEqualBlobs(benchmark::State& state)
{
    auto blob_a = make_my_blob();
    auto blob_b = make_my_blob();
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(blob_a == blob_b);
    }
}

static auto
get_unique_result(auto const& value)
{
    unique_hasher hasher;
    update_unique_hash(hasher, value);
    return hasher.get_result();
}

void
BM_UniqueHashGetResult(benchmark::State& state)
{
    auto the_blob = make_my_blob();
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(get_unique_result(the_blob));
    }
}

// Unique hash string e.g. used for disk cache digest
void
BM_UniqueHashGetString(benchmark::State& state)
{
    auto the_blob = make_my_blob();
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(get_unique_string_tmpl(the_blob));
    }
}

BENCHMARK(BM_BoostHash);
BENCHMARK(BM_CompareEqualBlobs);
BENCHMARK(BM_UniqueHashGetResult);
BENCHMARK(BM_UniqueHashGetString);
