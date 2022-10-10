#include <cradle/inner/core/id.h>

#include <map>
#include <unordered_map>

#include <catch2/catch.hpp>

#include "../support/ids.h"

using namespace cradle;

TEST_CASE("simple_id", "[inner][id]")
{
    test_different_ids(make_id(0), make_id(1));
}

TEST_CASE("simple_id_by_reference", "[inner][id]")
{
    int x = 0, y = 1;
    test_different_ids(make_id_by_reference(x), make_id_by_reference(y));
}

TEST_CASE("id_ref", "[inner][id]")
{
    test_different_ids(ref(make_captured_id(0)), ref(make_captured_id(1)));
}

TEST_CASE("captured_id basics", "[inner][id]")
{
    captured_id c;
    REQUIRE(!c.is_initialized());
    REQUIRE(!c.matches(make_id(0)));
    auto d = make_captured_id(0);
    REQUIRE(d.is_initialized());
    REQUIRE(d.matches(make_id(0)));
    REQUIRE(!d.matches(make_id(1)));
    REQUIRE(*d == make_id(0));
    d.clear();
    REQUIRE(!d.is_initialized());
}

TEST_CASE("captured_id operators", "[inner][id]")
{
    auto c = make_captured_id(0);
    captured_id d;
    REQUIRE(c != d);
    auto e = make_captured_id(0);
    REQUIRE(c == e);
    auto f = make_captured_id(1);
    REQUIRE(c != f);
    REQUIRE(c < f);
}

TEST_CASE("captured_id construction from raw pointer", "[inner][id]")
{
    captured_id c{new simple_id(87)};
    REQUIRE(c.is_initialized());
    REQUIRE(*c == make_id(87));
    c.clear();
    REQUIRE(!c.is_initialized());
}

TEST_CASE("captured_id copy construction", "[inner][id]")
{
    auto c = make_captured_id(0);
    captured_id d = c;
    REQUIRE(d == c);
    // Check that d is independent of changes in c.
    auto tmp0 = make_captured_id(1);
    swap(c, tmp0);
    REQUIRE(d != c);

    c.clear();
    captured_id e = c;
    REQUIRE(e == c);
    // Check that e is independent of changes in c.
    auto tmp1 = make_captured_id(1);
    swap(c, tmp1);
    REQUIRE(e != c);
}

TEST_CASE("captured_id move construction", "[inner][id]")
{
    auto c = make_captured_id(0);
    captured_id d = std::move(c);
    REQUIRE(d.matches(make_id(0)));

    captured_id e;
    e.clear();
    captured_id f = std::move(e);
    REQUIRE(!f.is_initialized());
}

TEST_CASE("captured_id copy assignment", "[inner][id]")
{
    auto c = make_captured_id(0);
    captured_id d;
    d = c;
    REQUIRE(d == c);
    // Check that d is independent of changes in c.
    auto tmp0 = make_captured_id(1);
    swap(c, tmp0);
    REQUIRE(d != c);

    c.clear();
    captured_id e;
    e = c;
    REQUIRE(e == c);
    // Check that e is independent of changes in c.
    auto tmp1 = make_captured_id(1);
    swap(c, tmp1);
    REQUIRE(e != c);
}

TEST_CASE("captured_id move assignment", "[inner][id]")
{
    auto c = make_captured_id(0);
    captured_id d;
    d = std::move(c);
    REQUIRE(d.matches(make_id(0)));

    captured_id e;
    e.clear();
    captured_id f;
    f = std::move(e);
    REQUIRE(!f.is_initialized());
}

TEST_CASE("combine_ids x2", "[inner][id]")
{
    auto a = combine_ids(make_id(0), make_id(1));
    auto b = combine_ids(make_id(1), make_id(2));
    test_different_ids(*a, *b);
}

TEST_CASE("combine_ids x3", "[inner][id]")
{
    auto a = combine_ids(make_id(0), make_id(1), make_id(2));
    auto b = combine_ids(make_id(1), make_id(2), make_id(3));
    test_different_ids(*a, *b);
}

TEST_CASE("combine_ids x4", "[inner][id]")
{
    auto a = combine_ids(make_id(0), make_id(1), make_id(2), make_id(3));
    auto b = combine_ids(make_id(1), make_id(2), make_id(3), make_id(4));
    test_different_ids(*a, *b);
}

TEST_CASE("map of IDs", "[inner][id]")
{
    auto zero = make_id(0);
    auto abc = make_id(std::string("abc"));
    auto one = make_id(1);
    auto another_one = make_id(1);

    std::map<id_interface const*, int, id_interface_pointer_less_than_test> m;
    m[&zero] = 0;
    m[&abc] = 123;
    REQUIRE(m.at(&zero) == 0);
    REQUIRE(m.at(&abc) == 123);
    REQUIRE(m.find(&one) == m.end());
    m[&one] = 1;
    REQUIRE(m.at(&one) == 1);
    REQUIRE(m.at(&another_one) == 1);
}

TEST_CASE("unordered_map of IDs", "[inner][id]")
{
    auto zero = make_id(0);
    auto abc = make_id(std::string("abc"));
    auto one = make_id(1);
    auto another_one = make_id(1);

    std::unordered_map<
        id_interface const*,
        int,
        id_interface_pointer_hash,
        id_interface_pointer_equality_test>
        m;
    m[&zero] = 0;
    m[&abc] = 123;
    REQUIRE(m.at(&zero) == 0);
    REQUIRE(m.at(&abc) == 123);
    REQUIRE(m.find(&one) == m.end());
    m[&one] = 1;
    REQUIRE(m.at(&one) == 1);
    REQUIRE(m.at(&another_one) == 1);
}
