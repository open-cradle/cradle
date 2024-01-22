#include <string>

// Boost.Crc triggers some warnings on MSVC.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4245)
#pragma warning(disable : 4701)
#include <boost/crc.hpp>
#pragma warning(pop)
#else
#include <boost/crc.hpp>
#endif

#include <benchmark/benchmark.h>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/hash.h>
#include <cradle/inner/encodings/lz4.h>
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

uint32_t
calc_crc32(void const* data, std::size_t byte_count)
{
    boost::crc_32_type crc;
    crc.process_bytes(data, byte_count);
    return crc.checksum();
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

void
BM_BoostCrc32(benchmark::State& state)
{
    auto the_blob = make_my_blob();
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(calc_crc32(the_blob.data(), the_blob.size()));
    }
}

void
BM_Lz4Compress(benchmark::State& state)
{
    auto the_blob = make_my_blob();
    byte_vector dest(lz4::max_compressed_size(the_blob.size()));
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(lz4::compress(
            dest.data(), dest.capacity(), the_blob.data(), the_blob.size()));
    }
}

void
BM_Lz4Decompress(benchmark::State& state)
{
    auto the_blob = make_my_blob();
    byte_vector compressed(lz4::max_compressed_size(the_blob.size()));
    auto compressed_size = lz4::compress(
        compressed.data(),
        compressed.capacity(),
        the_blob.data(),
        the_blob.size());
    byte_vector dest;
    dest.reserve(the_blob.size());

    for (auto _ : state)
    {
        lz4::decompress(
            dest.data(), dest.capacity(), compressed.data(), compressed_size);
    }
}

BENCHMARK(BM_BoostHash);
BENCHMARK(BM_CompareEqualBlobs);
BENCHMARK(BM_UniqueHashGetResult);
BENCHMARK(BM_UniqueHashGetString);
// Boost's CRC32 does not use hardware acceleration and is thus pretty slow;
// even slower than hardware-accelerated SHA256.
BENCHMARK(BM_BoostCrc32);
// make_my_blob() isn't a good input for (de-)compression benchmarks
BENCHMARK(BM_Lz4Compress);
BENCHMARK(BM_Lz4Decompress);
