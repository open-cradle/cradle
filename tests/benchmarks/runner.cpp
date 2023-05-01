#include <benchmark/benchmark.h>

#include "benchmark_support.h"
#include <cradle/inner/utilities/logging.h>

int
main(int argc, char** argv)
{
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
    {
        return 1;
    }

    // Can be overruled by setting SPDLOG_LEVEL, e.g.
    //   export SPDLOG_LEVEL=debug
    cradle::initialize_logging("warn");

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return cradle::check_benchmarks_skipped_with_error();
}
