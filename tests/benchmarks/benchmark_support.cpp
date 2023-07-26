#include <iostream>

#include <benchmark/benchmark.h>

#include "benchmark_support.h"

namespace cradle {

static int num_benchmarks_with_error{0};

void
handle_benchmark_exception(benchmark::State& state, std::string const& what)
{
    // SkipWithError() is being changed to take "std::string const&"
    state.SkipWithError(what.c_str());
    num_benchmarks_with_error += 1;
}

int
check_benchmarks_skipped_with_error()
{
    int exit_code = 0;
    if (num_benchmarks_with_error > 0)
    {
        std::cerr << num_benchmarks_with_error
                  << " benchmark test(s) were aborted with an error\n";
        exit_code = 1;
    }
    return exit_code;
}

} // namespace cradle
