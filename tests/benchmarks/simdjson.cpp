#include <mutex>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4189)
#include <simdjson.h>
#pragma warning(pop)
#else
#include <simdjson.h>
#endif

namespace {

std::vector<std::string> samples = {
    R"(
    {
        "disk_cache": {
            "directory": "/var/cache/cradle",
            "size_limit": 6000000000
        },
        "open": true
    }
    )",

    R"(
    [ true, false ]
    )",

    R"(
    [ 0, 1, 2 ]
    )"};

std::string const&
get_sample(int ix)
{
    return samples[ix % samples.size()];
}

// Create a simdjson parser (probably not holding many internal buffers).
void
BM_SimdJsonCreateParser(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(simdjson::dom::parser{});
    }
}

// Parse sample JSON using simdjson, allocating a dedicated parser for each
// sample.
void
BM_SimdJsonReallocateParser(benchmark::State& state)
{
    int i = 0;
    for (auto _ : state)
    {
        simdjson::dom::parser the_parser;
        std::string const& sample{get_sample(i)};
        benchmark::DoNotOptimize(
            the_parser.parse(sample.c_str(), sample.size()));
        ++i;
    }
}

// Parse sample JSON using simdjson, re-using the parser across all loops.
void
BM_SimdJsonOneParser(benchmark::State& state)
{
    simdjson::dom::parser the_parser;
    // Simulate an environment where multiple threads use a single parser and
    // access is controlled through a mutex.
    std::mutex the_mutex;
    int i = 0;
    for (auto _ : state)
    {
        std::string const& sample{get_sample(i)};
        std::scoped_lock<std::mutex> guard(the_mutex);
        benchmark::DoNotOptimize(
            the_parser.parse(sample.c_str(), sample.size()));
        ++i;
    }
}

} // namespace

BENCHMARK(BM_SimdJsonCreateParser);
BENCHMARK(BM_SimdJsonReallocateParser);
BENCHMARK(BM_SimdJsonOneParser);
