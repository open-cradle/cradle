#ifndef CRADLE_TESTS_SUPPORT_IDS_H
#define CRADLE_TESTS_SUPPORT_IDS_H

#include <memory>

#include <catch2/catch.hpp>

#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/id.h>

// Test all the relevant ID operations on a pair of equal IDs.
inline void
test_equal_ids(cradle::id_interface const& a, cradle::id_interface const& b)
{
    REQUIRE(a == b);
    REQUIRE(b == a);
    REQUIRE(!(a < b));
    REQUIRE(!(b < a));
    REQUIRE(a.hash() == b.hash());
    REQUIRE(get_unique_string(a) == get_unique_string(b));
}

// Test all the ID operations on a single ID.
template<class Id>
void
test_single_id(Id const& id)
{
    test_equal_ids(id, id);
}

// Test all the ID operations on a pair of different IDs.
template<class A, class B>
void
test_different_ids(A const& a, B const& b)
{
    test_single_id(a);
    test_single_id(b);
    REQUIRE(a != b);
    REQUIRE((a < b && !(b < a) || b < a && !(a < b)));
    REQUIRE(a.hash() != b.hash());
    REQUIRE(get_unique_string(a) != get_unique_string(b));
}

#endif
