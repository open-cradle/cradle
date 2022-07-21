#include <benchmark/benchmark.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

auto the_logger = spdlog::stdout_color_mt("cradle");

BENCHMARK_MAIN();
