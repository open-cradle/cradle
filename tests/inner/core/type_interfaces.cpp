#include <cradle/inner/core/type_interfaces.h>

#include <catch2/catch.hpp>

#include "../../support/regular.h"
#include <cradle/inner/utilities/text.h>

using std::optional;
using std::string;

using namespace cradle;

TEST_CASE("bool type interface (inner)", "[inner][types]")
{
    test_inner_regular_value_pair(false, true);
}

template<class Integer>
void
test_integer_interface()
{
    test_inner_regular_value_pair(Integer(0), Integer(1));

    REQUIRE(deep_sizeof(Integer(0)) == sizeof(Integer));
}

TEST_CASE("integer type interfaces (inner)", "[inner][types]")
{
    test_integer_interface<signed char>();
    test_integer_interface<unsigned char>();
    test_integer_interface<signed short>();
    test_integer_interface<unsigned short>();
    test_integer_interface<signed int>();
    test_integer_interface<unsigned int>();
    test_integer_interface<signed long>();
    test_integer_interface<unsigned long>();
    test_integer_interface<signed long long>();
    test_integer_interface<unsigned long long>();
}

template<class Float>
void
test_float_interface()
{
    test_inner_regular_value_pair(Float(0.5), Float(1.5));

    REQUIRE(deep_sizeof(Float(0)) == sizeof(Float));
}

TEST_CASE("floating point type interfaces (inner)", "[inner][types]")
{
    test_float_interface<float>();
    test_float_interface<double>();
}

TEST_CASE("string type interface (inner)", "[inner][types]")
{
    test_inner_regular_value_pair(string("hello"), string("world!"));

    REQUIRE(deep_sizeof(string("hello")) == deep_sizeof(string()) + 5);
}

TEST_CASE("blob type interface (inner)", "[inner][types]")
{
    std::byte blob_data[] = {std::byte{0}, std::byte{1}};

    INFO("Test blobs of the different sizes.");
    test_inner_regular_value_pair(
        make_static_blob(blob_data, 1), make_static_blob(blob_data, 2));

    INFO("Test blobs of the same size but with different data.");
    test_inner_regular_value_pair(
        make_static_blob(blob_data, 1), make_static_blob(blob_data + 1, 1));
}

TEST_CASE("optional type interface (inner)", "[inner][types]")
{
    // Test an optional with a value.
    test_inner_regular_value_pair(
        some(string("hello")), some(string("world!")));

    REQUIRE(
        deep_sizeof(some(string("hello")))
        == sizeof(optional<string>) + deep_sizeof(string()) + 5);

    // Test an empty optional.
    test_inner_regular_value(optional<string>());
    REQUIRE(deep_sizeof(optional<string>()) == sizeof(optional<string>));
}

TEST_CASE("vector type interface (inner)", "[inner][types]")
{
    test_inner_regular_value_pair(
        std::vector<int>({0, 1}), std::vector<int>({1}));

    REQUIRE(
        deep_sizeof(std::vector<int>({0, 1}))
        == deep_sizeof(std::vector<int>()) + deep_sizeof(0) + deep_sizeof(1));
}

TEST_CASE("map type interface (inner)", "[inner][types]")
{
    test_inner_regular_value(std::map<int, int>({}));

    test_inner_regular_value_pair(
        std::map<int, int>({{0, 1}, {1, 2}}),
        std::map<int, int>({{1, 2}, {2, 5}, {3, 7}}));

    REQUIRE(
        deep_sizeof(std::map<int, int>({{0, 1}}))
        == deep_sizeof(std::map<int, int>()) + deep_sizeof(0)
               + deep_sizeof(1));
}
