#include <string>

#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/unique_hash.h>
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

void
BM_UniqueHash(benchmark::State& state)
{
    auto the_blob = make_my_blob();
    unique_hasher the_hasher;
    for (auto _ : state)
    {
        update_unique_hash(the_hasher, the_blob);
    }
}

BENCHMARK(BM_BoostHash);
BENCHMARK(BM_CompareEqualBlobs);
BENCHMARK(BM_UniqueHash);
