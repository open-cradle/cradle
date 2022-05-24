#define CATCH_CONFIG_MAIN

// Ask Catch to dump memory leaks under Windows.
#ifdef _WIN32
#define CATCH_CONFIG_WINDOWS_CRTDBG
#endif

// Disable coloring because it doesn't seem to work properly on Windows.
#define CATCH_CONFIG_COLOUR_NONE

// Allowing catch to support nullptr causes duplicate definitions for some
// things.
#define CATCH_CONFIG_CPP11_NO_NULLPTR

// Enable benchmarking. Not enabled elsewhere because documentation says it
// "has a significant effect on compilation speed".
#define CATCH_CONFIG_ENABLE_BENCHMARKING

// The Catch "main" code triggers these in Visual C++.
#if defined(_MSC_VER)
#pragma warning(disable : 4244)
#pragma warning(disable : 4702)
#endif

#include <catch2/catch.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

auto the_logger = spdlog::stdout_color_mt("cradle");
