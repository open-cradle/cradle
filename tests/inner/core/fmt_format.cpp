#include <catch2/catch.hpp>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/core/type_definitions.h>

using namespace cradle;

static void
test_formatter(blob const& x, std::string const& expected)
{
    std::string actual = fmt::format("{}", x);
    REQUIRE(actual == expected);
}

TEST_CASE("format empty blob", "[inner][fmt]")
{
    auto empty_blob = make_string_literal_blob("");
    test_formatter(empty_blob, "0-bytes blob");
}

TEST_CASE("format small printable blob", "[inner][fmt]")
{
    auto small_blob = make_string_literal_blob("small blob");
    test_formatter(small_blob, "10-bytes blob: small blob");
}

TEST_CASE("format large printable blob", "[inner][fmt]")
{
    byte_vector large_vector(16384);
    auto large_blob{make_blob(large_vector)};
    test_formatter(
        large_blob,
        "16384-bytes blob: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ... "
        "00 00 00 00");
}

TEST_CASE("format blob with unprintable characters", "[inner][fmt]")
{
    auto unprintable_blob = make_string_literal_blob("\xf1wxyz");
    test_formatter(unprintable_blob, "5-bytes blob: f1 77 78 79 7a");
}
