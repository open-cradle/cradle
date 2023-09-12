#include <cradle/typing/core/dynamic.h>

#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/utilities/text.h>
#include <cradle/typing/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("value_type streaming", "[core][dynamic]")
{
    REQUIRE(lexical_cast<string>(value_type::NIL) == "nil");
    REQUIRE(lexical_cast<string>(value_type::BOOLEAN) == "boolean");
    REQUIRE(lexical_cast<string>(value_type::INTEGER) == "integer");
    REQUIRE(lexical_cast<string>(value_type::FLOAT) == "float");
    REQUIRE(lexical_cast<string>(value_type::STRING) == "string");
    REQUIRE(lexical_cast<string>(value_type::BLOB) == "blob");
    REQUIRE(lexical_cast<string>(value_type::DATETIME) == "datetime");
    REQUIRE(lexical_cast<string>(value_type::ARRAY) == "array");
    REQUIRE(lexical_cast<string>(value_type::MAP) == "map");
    REQUIRE_THROWS_AS(
        lexical_cast<string>(value_type(-1)), invalid_enum_value);
}

TEST_CASE("dynamic type checking", "[core][dynamic]")
{
    try
    {
        check_type(value_type::NIL, value_type::BOOLEAN);
        FAIL("no exception thrown");
    }
    catch (type_mismatch& e)
    {
        REQUIRE(
            get_required_error_info<expected_value_type_info>(e)
            == value_type::NIL);
        REQUIRE(
            get_required_error_info<actual_value_type_info>(e)
            == value_type::BOOLEAN);
    }

    REQUIRE_NOTHROW(check_type(value_type::NIL, value_type::NIL));
}

TEST_CASE("dynamic initializer lists", "[core][dynamic]")
{
    // Test a simple initializer list.
    REQUIRE(
        (dynamic{dynamic{0.}, dynamic{1.}, dynamic{2.}})
        == dynamic{dynamic_array{dynamic{0.}, dynamic{1.}, dynamic{2.}}});

    // Test that lists that look like maps are interpretted as maps.
    REQUIRE(
        (dynamic{{dynamic{"foo"}, dynamic{0.}}, {dynamic{"bar"}, dynamic{1.}}})
        == dynamic{dynamic_map{
            {dynamic{"foo"}, dynamic{0.}}, {dynamic{"bar"}, dynamic{1.}}}});

    // Test that the conversion to map only happens with string keys.
    REQUIRE(
        (dynamic{{dynamic{"foo"}, dynamic{0.}}, {dynamic{0.}, dynamic{1.}}})
        == dynamic{
            dynamic{dynamic_array{dynamic{"foo"}, dynamic{0.}}},
            dynamic{dynamic_array{dynamic{0.}, dynamic{1.}}}});
}

TEST_CASE("dynamic type interface", "[core][dynamic]")
{
    test_regular_value_pair(dynamic{false}, dynamic{true});

    test_regular_value_pair(dynamic{integer(0)}, dynamic{integer(1)});

    test_regular_value_pair(dynamic{0.}, dynamic{1.});

    test_regular_value_pair(dynamic{string("bar")}, dynamic{string("foo")});

    std::byte blob_data[] = {std::byte{0}, std::byte{1}};
    test_regular_value_pair(
        dynamic{make_static_blob(blob_data, 1)},
        dynamic{make_static_blob(blob_data, 2)});

    test_regular_value_pair(
        dynamic{boost::posix_time::ptime(
            boost::gregorian::date(2017, boost::gregorian::Apr, 26),
            boost::posix_time::time_duration(1, 2, 3))},
        dynamic{boost::posix_time::ptime(
            boost::gregorian::date(2017, boost::gregorian::Apr, 26),
            boost::posix_time::time_duration(1, 2, 4))});

    test_regular_value_pair(
        dynamic{{dynamic{0.}, dynamic{1.}}},
        dynamic{{dynamic{1.}, dynamic{2.}}});

    test_regular_value_pair(
        dynamic{{dynamic{0.}, dynamic{1.}}},
        dynamic{{dynamic{1.}, dynamic{2.}}});
}

TEST_CASE("dynamic deep_sizeof", "[core][dynamic]")
{
    REQUIRE(deep_sizeof(dynamic{nil}) == sizeof(dynamic) + deep_sizeof(nil));
    REQUIRE(
        deep_sizeof(dynamic{false}) == sizeof(dynamic) + deep_sizeof(false));
    REQUIRE(
        deep_sizeof(dynamic{0}) == sizeof(dynamic) + deep_sizeof(integer(0)));
    REQUIRE(deep_sizeof(dynamic{0.}) == sizeof(dynamic) + deep_sizeof(0.));
    REQUIRE(
        deep_sizeof(dynamic{string("foo")})
        == sizeof(dynamic) + deep_sizeof(string("foo")));
    std::byte blob_data[] = {std::byte{0}, std::byte{1}};
    auto blob = make_static_blob(blob_data, 2);
    REQUIRE(deep_sizeof(dynamic{blob}) == sizeof(dynamic) + deep_sizeof(blob));
    auto time = boost::posix_time::ptime(
        boost::gregorian::date(2017, boost::gregorian::Apr, 26),
        boost::posix_time::time_duration(1, 2, 3));
    REQUIRE(deep_sizeof(dynamic{time}) == sizeof(dynamic) + deep_sizeof(time));
    auto array = dynamic_array{{dynamic{3.}, dynamic{1.}, dynamic{2.}}};
    REQUIRE(
        deep_sizeof(dynamic{array}) == sizeof(dynamic) + deep_sizeof(array));
    auto map
        = dynamic_map{{dynamic{0.}, dynamic{1.}}, {dynamic{1.}, dynamic{2.}}};
    REQUIRE(deep_sizeof(dynamic{map}) == sizeof(dynamic) + deep_sizeof(map));

    REQUIRE(deep_sizeof(dynamic_array{}) == sizeof(dynamic_array));
    REQUIRE(deep_sizeof(dynamic_map{}) == sizeof(dynamic_map));
}

TEST_CASE("empty array/map equivalence", "[core][dynamic]")
{
    {
        INFO(
            "Dynamic values containing empty maps can be treated as empty "
            "arrays.")
        REQUIRE(
            from_dynamic<std::vector<double>>(dynamic{dynamic_map{}}).empty());
        INFO("This doesn't work for non-empty maps.")
        REQUIRE_THROWS(from_dynamic<std::vector<double>>(
            dynamic{dynamic_map{{dynamic{0.}, dynamic{1.}}}}));
    }
    {
        INFO(
            "Dynamic values containing empty arrays can be treated as "
            "empty "
            "maps.")
        REQUIRE(
            (from_dynamic<std::map<double, double>>(dynamic{dynamic_array{}})
                 .empty()));
        INFO("This doesn't work for non-empty arrays.")
        REQUIRE_THROWS((from_dynamic<std::map<double, double>>(
            dynamic{dynamic_array{{dynamic{1.}}}})));
    }
}

TEST_CASE("get_field", "[core][dynamic]")
{
    auto map = dynamic_map{
        {dynamic{"a"}, dynamic{12.}}, {dynamic{"b"}, dynamic{false}}};

    // Try getting both fields.
    REQUIRE(get_field(map, "a") == dynamic{12.});
    REQUIRE(get_field(map, "b") == dynamic{false});

    // Try a missing field.
    try
    {
        get_field(map, "c");
        FAIL("no exception thrown");
    }
    catch (missing_field& e)
    {
        REQUIRE(get_required_error_info<field_name_info>(e) == "c");
    }
}

TEST_CASE("get_union_tag", "[core][dynamic]")
{
    // Try getting the type from a proper union dynamic.
    REQUIRE(
        get_union_tag(dynamic_map{
            {dynamic{"a"}, dynamic{12.}},
        })
        == dynamic{"a"});

    // Try with an empty map.
    try
    {
        get_union_tag(dynamic_map{});
        FAIL("no exception thrown");
    }
    catch (multifield_union&)
    {
    }

    // Try with a map with too many fields.
    try
    {
        get_union_tag(dynamic_map{
            {dynamic{"a"}, dynamic{12.}}, {dynamic{"b"}, dynamic{false}}});
        FAIL("no exception thrown");
    }
    catch (multifield_union&)
    {
    }
}

TEST_CASE("dynamic operators", "[core][dynamic]")
{
    dynamic a;
    dynamic b(integer(0));
    dynamic c(integer(1));

    REQUIRE(a == a);
    REQUIRE(b == b);
    REQUIRE(c == c);

    REQUIRE(a != b);
    REQUIRE(b != c);
    REQUIRE(a != c);

    REQUIRE(a < b);
    REQUIRE(b < c);
    REQUIRE(a < c);

    REQUIRE(b > a);
    REQUIRE(c > b);
    REQUIRE(c > a);

    REQUIRE(a <= a);
    REQUIRE(b <= b);
    REQUIRE(c <= c);

    REQUIRE(a <= b);
    REQUIRE(b <= c);
    REQUIRE(a <= c);

    REQUIRE(a >= a);
    REQUIRE(b >= b);
    REQUIRE(c >= c);

    REQUIRE(b >= a);
    REQUIRE(c >= b);
    REQUIRE(c >= a);
}

TEST_CASE("dynamic value coercion", "[core][dynamic]")
{
    auto type_dictionary = std::map<api_named_type_reference, api_type_info>(
        {{make_api_named_type_reference("my_app", "int"),
          make_api_type_info_with_integer_type(api_integer_type())},
         {make_api_named_type_reference("my_app", "float"),
          make_api_type_info_with_float_type(api_float_type())}});
    std::function<cppcoro::task<api_type_info>(
        api_named_type_reference const& ref)>
        look_up_named_type = [&](api_named_type_reference const& ref)
        -> cppcoro::task<api_type_info> { co_return type_dictionary[ref]; };
    auto coerce_value = [&](api_type_info const& type, auto&& value) {
        return cppcoro::sync_wait(
            cradle::coerce_value(look_up_named_type, type, value));
    };
    auto value_requires_coercion
        = [&](api_type_info const& type, auto const& value) -> bool {
        return cppcoro::sync_wait(cradle::detail::value_requires_coercion(
            look_up_named_type, type, value));
    };

    auto nil_type = make_api_type_info_with_nil_type(api_nil_type());
    REQUIRE(!value_requires_coercion(nil_type, dynamic{nil}));
    REQUIRE(coerce_value(nil_type, dynamic{nil}) == dynamic{nil});
    REQUIRE_THROWS(coerce_value(nil_type, dynamic{false}));

    auto boolean_type
        = make_api_type_info_with_boolean_type(api_boolean_type());
    REQUIRE(!value_requires_coercion(boolean_type, dynamic{false}));
    REQUIRE(coerce_value(boolean_type, dynamic{false}) == dynamic{false});
    REQUIRE_THROWS(coerce_value(boolean_type, dynamic{nil}));

    auto integer_type
        = make_api_type_info_with_integer_type(api_integer_type());
    REQUIRE(!value_requires_coercion(integer_type, dynamic{0}));
    REQUIRE(coerce_value(integer_type, dynamic{0}) == dynamic{0});
    // Test that doubles can be coerced to integers iff they're actually
    // integers.
    REQUIRE(value_requires_coercion(integer_type, dynamic{double(0)}));
    REQUIRE(coerce_value(integer_type, dynamic{double(0)}) == dynamic{0});
    REQUIRE_THROWS(coerce_value(integer_type, dynamic{double(0.5)}));
    REQUIRE_THROWS(coerce_value(integer_type, dynamic{false}));

    auto float_type = make_api_type_info_with_float_type(api_float_type());
    REQUIRE(!value_requires_coercion(float_type, dynamic{double(0)}));
    REQUIRE(value_requires_coercion(float_type, dynamic{0}));
    REQUIRE(
        coerce_value(float_type, dynamic{double(0)}) == dynamic{double(0)});
    REQUIRE(
        coerce_value(float_type, dynamic{double(0.5)})
        == dynamic{double(0.5)});
    // Test that integers can be coerced to doubles.
    REQUIRE(coerce_value(float_type, dynamic{0}) == dynamic{double(0)});
    REQUIRE_THROWS(coerce_value(float_type, dynamic{false}));

    // Test that we can do all this through named types.
    auto named_integer_type = make_api_type_info_with_named_type(
        make_api_named_type_reference("my_app", "int"));
    auto named_float_type = make_api_type_info_with_named_type(
        make_api_named_type_reference("my_app", "float"));
    REQUIRE(
        coerce_value(named_integer_type, dynamic{double(0)}) == dynamic{0});
    REQUIRE_THROWS(coerce_value(named_integer_type, dynamic{double(0.5)}));
    REQUIRE_THROWS(coerce_value(named_integer_type, dynamic{false}));
    REQUIRE(coerce_value(named_float_type, dynamic{0}) == dynamic{double(0)});
    REQUIRE(
        coerce_value(named_float_type, dynamic{double(0.5)})
        == dynamic{double(0.5)});
    REQUIRE(!value_requires_coercion(named_integer_type, dynamic{0}));
    REQUIRE(value_requires_coercion(named_integer_type, dynamic{double(0)}));

    auto string_type = make_api_type_info_with_string_type(api_string_type());
    REQUIRE(
        coerce_value(string_type, dynamic{string("xyz")})
        == dynamic{string("xyz")});
    REQUIRE(!value_requires_coercion(string_type, dynamic{string("xyz")}));
    REQUIRE_THROWS(coerce_value(string_type, dynamic{false}));

    auto blob_type = make_api_type_info_with_blob_type(api_blob_type());
    auto test_blob = make_string_literal_blob("abc");
    REQUIRE(!value_requires_coercion(blob_type, dynamic{test_blob}));
    REQUIRE(coerce_value(blob_type, dynamic{test_blob}) == dynamic{test_blob});
    REQUIRE_THROWS(coerce_value(blob_type, dynamic{false}));

    auto datetime_type
        = make_api_type_info_with_datetime_type(api_datetime_type());
    auto test_datetime = ptime(
        date(2017, boost::gregorian::Apr, 26),
        boost::posix_time::time_duration(1, 2, 3));
    REQUIRE(!value_requires_coercion(datetime_type, dynamic{test_datetime}));
    REQUIRE(
        coerce_value(datetime_type, dynamic{test_datetime})
        == dynamic{test_datetime});
    REQUIRE_THROWS(coerce_value(datetime_type, dynamic{false}));

    auto integer_array_type = make_api_type_info_with_array_type(
        make_api_array_info(none, integer_type));
    auto test_integer_array
        = dynamic_array{dynamic{2}, dynamic{0}, dynamic{3}};
    auto float_array_type = make_api_type_info_with_array_type(
        make_api_array_info(none, float_type));
    auto test_float_array = dynamic_array{
        dynamic{double(2)}, dynamic{double(0)}, dynamic{double(3)}};
    auto test_boolean_array = dynamic_array{dynamic{false}, dynamic{true}};
    // Test that the double <-> integer coercions work within arrays.
    REQUIRE(!value_requires_coercion(
        integer_array_type, dynamic{test_integer_array}));
    REQUIRE(
        coerce_value(integer_array_type, dynamic{test_integer_array})
        == dynamic{test_integer_array});
    REQUIRE(value_requires_coercion(
        float_array_type, dynamic{test_integer_array}));
    REQUIRE(
        coerce_value(float_array_type, dynamic{test_integer_array})
        == dynamic{test_float_array});
    REQUIRE(value_requires_coercion(
        integer_array_type, dynamic{test_float_array}));
    REQUIRE(
        coerce_value(integer_array_type, dynamic{test_float_array})
        == dynamic{test_integer_array});
    REQUIRE(
        !value_requires_coercion(float_array_type, dynamic{test_float_array}));
    REQUIRE(
        coerce_value(float_array_type, dynamic{test_float_array})
        == dynamic{test_float_array});
    REQUIRE_THROWS(coerce_value(float_array_type, dynamic{false}));
    REQUIRE_THROWS(
        coerce_value(float_array_type, dynamic{test_boolean_array}));

    auto enum_type = make_api_type_info_with_enum_type(api_enum_info(
        {{"def", make_api_enum_value_info("xyz")},
         {"abc", make_api_enum_value_info("qrs")}}));
    REQUIRE(!value_requires_coercion(enum_type, dynamic{"def"}));
    REQUIRE(coerce_value(enum_type, dynamic{"def"}) == dynamic{"def"});
    REQUIRE(!value_requires_coercion(enum_type, dynamic{"abc"}));
    REQUIRE(coerce_value(enum_type, dynamic{"abc"}) == dynamic{"abc"});
    REQUIRE_THROWS(coerce_value(enum_type, dynamic{"ijk"}));

    auto optional_type = make_api_type_info_with_optional_type(integer_type);
    // Test that the double <-> integer coercions work within optionals.
    REQUIRE(!value_requires_coercion(
        optional_type, dynamic{{dynamic{"some"}, dynamic{integer(0)}}}));
    REQUIRE(
        coerce_value(
            optional_type, dynamic{{dynamic{"some"}, dynamic{integer(0)}}})
        == dynamic{{dynamic{"some"}, dynamic{integer(0)}}});
    REQUIRE(value_requires_coercion(
        optional_type, dynamic{{dynamic{"some"}, dynamic{double(0)}}}));
    REQUIRE(
        coerce_value(
            optional_type, dynamic{{dynamic{"some"}, dynamic{double(0)}}})
        == dynamic{{dynamic{"some"}, dynamic{integer(0)}}});
    REQUIRE(!value_requires_coercion(
        optional_type, dynamic{{dynamic{"none"}, dynamic{nil}}}));
    REQUIRE(
        coerce_value(optional_type, dynamic{{dynamic{"none"}, dynamic{nil}}})
        == dynamic{{dynamic{"none"}, dynamic{nil}}});
    REQUIRE_THROWS(coerce_value(
        optional_type, dynamic{{dynamic{"some"}, dynamic{"abc"}}}));
    REQUIRE_THROWS(coerce_value(optional_type, dynamic{false}));

    auto map_type = make_api_type_info_with_map_type(
        make_api_map_info(float_type, integer_type));
    // Test that the double <-> integer coercions work within maps.
    REQUIRE(!value_requires_coercion(
        map_type,
        dynamic{dynamic_map{{dynamic{double(0)}, dynamic{integer(0)}}}}));
    REQUIRE(
        coerce_value(
            map_type,
            dynamic{dynamic_map{{dynamic{double(0)}, dynamic{integer(0)}}}})
        == dynamic{dynamic_map{{dynamic{double(0)}, dynamic{integer(0)}}}});
    REQUIRE(value_requires_coercion(
        map_type,
        dynamic{dynamic_map{{dynamic{integer(1)}, dynamic{integer(0)}}}}));
    REQUIRE(value_requires_coercion(
        map_type,
        dynamic{dynamic_map{{dynamic{double(1)}, dynamic{double(0)}}}}));
    REQUIRE(value_requires_coercion(
        map_type,
        dynamic{dynamic_map{{dynamic{integer(1)}, dynamic{double(0)}}}}));
    REQUIRE(
        coerce_value(
            map_type,
            dynamic{dynamic_map{{dynamic{integer(1)}, dynamic{double(0)}}}})
        == dynamic{dynamic_map{{dynamic{double(1)}, dynamic{integer(0)}}}});
    REQUIRE_THROWS(coerce_value(
        optional_type,
        dynamic{dynamic_map{{dynamic{"abc"}, dynamic{"def"}}}}));
    REQUIRE_THROWS(coerce_value(optional_type, dynamic{false}));

    auto struct_type
        = make_api_type_info_with_structure_type(api_structure_info(
            {{"def", make_api_structure_field_info("ijk", float_type, none)},
             {"abc",
              make_api_structure_field_info("xyz", integer_type, true)}}));
    // Test that the double <-> integer coercions work within structures.
    REQUIRE(value_requires_coercion(
        struct_type,
        dynamic{
            {dynamic{"abc"}, dynamic{integer(0)}},
            {dynamic{"def"}, dynamic{integer(0)}}}));
    REQUIRE(
        coerce_value(
            struct_type,
            dynamic{
                {dynamic{"abc"}, dynamic{integer(0)}},
                {dynamic{"def"}, dynamic{integer(0)}}})
        == dynamic{
            {dynamic{"def"}, dynamic{double(0)}},
            {dynamic{"abc"}, dynamic{integer(0)}}});
    REQUIRE(value_requires_coercion(
        struct_type,
        dynamic{
            {dynamic{"def"}, dynamic{double(0.5)}},
            {dynamic{"abc"}, dynamic{double(1)}}}));
    REQUIRE(
        coerce_value(
            struct_type,
            dynamic{
                {dynamic{"def"}, dynamic{double(0.5)}},
                {dynamic{"abc"}, dynamic{double(1)}}})
        == dynamic{
            {dynamic{"def"}, dynamic{double(0.5)}},
            {dynamic{"abc"}, dynamic{integer(1)}}});
    REQUIRE(!value_requires_coercion(
        struct_type,
        dynamic{
            {dynamic{"def"}, dynamic{double(0.5)}},
            {dynamic{"abc"}, dynamic{integer(1)}}}));
    REQUIRE_THROWS(coerce_value(
        struct_type, dynamic{dynamic_map{{dynamic{"abc"}, dynamic{"xyz"}}}}));
    REQUIRE_THROWS(coerce_value(
        struct_type,
        dynamic{
            {dynamic{"def"}, dynamic{"xyz"}},
            {dynamic{"abc"}, dynamic{double(1)}}}));
    REQUIRE_THROWS(coerce_value(struct_type, dynamic{false}));

    auto union_type = make_api_type_info_with_union_type(api_union_info(
        {{"def", make_api_union_member_info("ijk", float_type)},
         {"abc", make_api_union_member_info("xyz", integer_type)}}));
    // Test that the double <-> integer coercions work within unions.
    REQUIRE(value_requires_coercion(
        union_type, dynamic{{dynamic{"def"}, dynamic{integer(0)}}}));
    REQUIRE(
        coerce_value(
            union_type, dynamic{{dynamic{"def"}, dynamic{integer(0)}}})
        == dynamic{{dynamic{"def"}, dynamic{double(0)}}});
    REQUIRE(!value_requires_coercion(
        union_type, dynamic{{dynamic{"abc"}, dynamic{integer(0)}}}));
    REQUIRE(
        coerce_value(
            union_type, dynamic{{dynamic{"abc"}, dynamic{integer(0)}}})
        == dynamic{{dynamic{"abc"}, dynamic{integer(0)}}});
    REQUIRE(!value_requires_coercion(
        union_type, dynamic{{dynamic{"def"}, dynamic{double(0.5)}}}));
    REQUIRE(
        coerce_value(
            union_type, dynamic{{dynamic{"def"}, dynamic{double(0.5)}}})
        == dynamic{{dynamic{"def"}, dynamic{double(0.5)}}});
    REQUIRE(value_requires_coercion(
        union_type, dynamic{{dynamic{"abc"}, dynamic{double(1)}}}));
    REQUIRE(
        coerce_value(union_type, dynamic{{dynamic{"abc"}, dynamic{double(1)}}})
        == dynamic{{dynamic{"abc"}, dynamic{integer(1)}}});
    REQUIRE_THROWS(coerce_value(
        union_type,
        dynamic{
            {dynamic{"abc"}, dynamic{integer(0)}},
            {dynamic{"def"}, dynamic{integer(0)}}}));
    REQUIRE_THROWS(coerce_value(
        union_type, dynamic{{dynamic{"xyz"}, dynamic{double(1)}}}));
    REQUIRE_THROWS(
        coerce_value(union_type, dynamic{{dynamic{"abc"}, dynamic{"xyz"}}}));
    REQUIRE_THROWS(coerce_value(struct_type, dynamic{false}));
}
