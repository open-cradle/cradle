#define CATCH_CONFIG_RUNNER

// Ask Catch to dump memory leaks under Windows.
#ifdef _WIN32
#define CATCH_CONFIG_WINDOWS_CRTDBG
#endif

// Disable coloring because it doesn't seem to work properly on Windows.
#define CATCH_CONFIG_COLOUR_NONE

// Allowing catch to support nullptr causes duplicate definitions for some
// things.
#define CATCH_CONFIG_CPP11_NO_NULLPTR

// Uncomment to have one-line messages
// #define CATCH_CONFIG_DEFAULT_REPORTER "compact"

// The Catch "main" code triggers this in Visual C++.
#if defined(_MSC_VER)
#pragma warning(disable : 4244)
#endif

#include <catch2/catch.hpp>

#include <cradle/inner/utilities/logging.h>

int
main(int argc, char* argv[])
{
    // Can be overruled by setting SPDLOG_LEVEL, e.g.
    //   export SPDLOG_LEVEL=debug
    cradle::initialize_logging("warn");

    return Catch::Session().run(argc, argv);
}
