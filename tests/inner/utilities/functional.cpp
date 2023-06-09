#include <cradle/inner/utilities/functional.h>

#include <catch2/catch.hpp>

using namespace cradle;

int
invoke_function_views(function_view<int(bool)> a, function_view<int(int)> b)
{
    return a(true) + b(12);
}

TEST_CASE("function_view", "[core][utilities]")
{
    REQUIRE(
        invoke_function_views(
            [](bool x) { return x ? 1 : 2; }, [](int x) { return x / 2; })
        == 7);
    REQUIRE(
        invoke_function_views(
            [](bool x) { return x ? 3 : 0; }, [](int x) { return x * 2; })
        == 27);
}
