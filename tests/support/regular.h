#ifndef CRADLE_TESTS_SUPPORT_REGULAR_H
#define CRADLE_TESTS_SUPPORT_REGULAR_H

#include <optional>
#include <utility>

#include <boost/concept_check.hpp>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/type_interfaces.h>

using std::optional;
using std::string;

namespace cradle {

// The Regular concept describes CRADLE regular types.
template<class T>
struct InnerRegular : boost::DefaultConstructible<T>,
                      boost::Assignable<T>,
                      boost::CopyConstructible<T>,
                      boost::EqualityComparable<T>,
                      boost::LessThanComparable<T>
{
    BOOST_CONCEPT_USAGE(InnerRegular)
    {
        // T must allow querying its size.
        using cradle::deep_sizeof;
        check_same_type(size_t(), deep_sizeof(t));

        // T must allow swapping.
        using std::swap;
        swap(t, t);
    }

 private:
    T t;

    // Check that the types of the two arguments are the same.
    // (Type deduction will fail if they're not.)
    template<class U>
    void
    check_same_type(U const&, U const&)
    {
    }
};

// Test that a type correctly implements the CRADLE Regular type interface for
// the given value. Note that this only tests the portions of the interface
// that are relevant for a single value. (In particular, it's intended to work
// for types that only have one possible value.)
template<class T>
void
test_inner_regular_value(T const& x)
{
    BOOST_CONCEPT_ASSERT((InnerRegular<T>) );

    {
        INFO("Copy construction should produce an equal value.")
        T y = x;
        REQUIRE(y == x);
    }

    {
        INFO("Assignment should produce an equal value.")
        T y;
        y = x;
        REQUIRE(y == x);
    }

    {
        INFO("std::swap should swap values.")

        T default_initialized = T();

        T y = x;
        T z = default_initialized;
        REQUIRE(y == x);
        REQUIRE(z == default_initialized);

        using std::swap;
        swap(y, z);
        REQUIRE(z == x);
        REQUIRE(y == default_initialized);

        INFO("A second std::swap should restore the original values.")
        swap(y, z);
        REQUIRE(y == x);
        REQUIRE(z == default_initialized);
    }
}

// Test that a type correctly implements the CRADLE Regular type interface for
// the given pair of values. This does additional tests that require two
// different values. It assumes that :x < :y and that the two produce different
// hash values.
template<class T>
void
test_inner_regular_value_pair(T const& x, T const& y)
{
    // Test the values individually.
    test_inner_regular_value(x);
    test_inner_regular_value(y);

    // :test_pair accepts a second pair of values (:a, :b) which is meant to
    // match the original pair (:x, :y). It tests that this is true and also
    // tests the original assumptions about the relationship between :x and :y.
    auto test_pair = [&](T const& a, T const& b) {
        REQUIRE(a != b);
        REQUIRE(invoke_hash(a) != invoke_hash(b));
        REQUIRE((a < b));
        REQUIRE(a == x);
        REQUIRE(b == y);
    };

    // Test the original values.
    test_pair(x, y);

    // Test copy construction, assignment, and swapping.
    {
        using std::swap;
        T a = x;
        T b = y;
        test_pair(a, b);
        b = x;
        a = y;
        test_pair(b, a);
        swap(a, b);
        test_pair(a, b);
        swap(a, b);
        test_pair(b, a);
    }
}

} // namespace cradle

#endif
