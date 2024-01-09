#include <cppcoro/sync_wait.hpp>

#include "../../support/thinknode.h"
#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/sha256_hash_id.h>
#include <cradle/inner/utilities/environment.h>
#include <cradle/typing/core/unique_hash.h>
#include <cradle/typing/utilities/testing.h>
#include <cradle/websocket/calculations.h>
#include <cradle/websocket/messages.hpp>

using namespace cradle;

namespace {

// Test all the relevant ID operations on a pair of equal captured_id's.
static void
test_equal_ids(captured_id const& a, captured_id const& b)
{
    REQUIRE(a == b);
    REQUIRE(b == a);
    REQUIRE(!(a < b));
    REQUIRE(!(b < a));
    REQUIRE(a.hash() == b.hash());
    auto a_string = get_unique_string(*a);
    auto b_string = get_unique_string(*b);
    REQUIRE(a_string == b_string);
}

// Test all the ID operations on a pair of different captured_id's.
void
test_different_ids(
    captured_id const& a, captured_id const& b, bool ignore_strings = false)
{
    REQUIRE(a != b);
    REQUIRE((a < b && !(b < a) || b < a && !(a < b)));
    REQUIRE(a.hash() != b.hash());
    auto a_string = get_unique_string(*a);
    auto b_string = get_unique_string(*b);
    // Raw function pointers are implicitly converted to bool
    if (!ignore_strings)
    {
        REQUIRE(a_string != b_string);
    }
}

int
add(int x, int y)
{
    return x + y;
}

int
subtract(int x, int y)
{
    return x - y;
}

} // namespace

TEST_CASE("function IDs", "[calcs][ws]")
{
    auto lambda_add = [](int x, int y) { return x + y; };
    auto lambda_subtract = [](int x, int y) { return x - y; };

    test_equal_ids(make_function_id(add), make_function_id(add));
    test_equal_ids(make_function_id(subtract), make_function_id(subtract));
    test_different_ids(
        make_function_id(add), make_function_id(subtract), true);

    test_equal_ids(make_function_id(lambda_add), make_function_id(lambda_add));
    test_equal_ids(
        make_function_id(lambda_subtract), make_function_id(lambda_subtract));
    test_different_ids(
        make_function_id(lambda_add), make_function_id(lambda_subtract));

    test_different_ids(make_function_id(add), make_function_id(lambda_add));
    test_different_ids(
        make_function_id(subtract), make_function_id(lambda_subtract));

    captured_id captured0(make_function_id(add));
    REQUIRE(captured0.matches(*make_function_id(add)));
    REQUIRE(!captured0.matches(*make_function_id(subtract)));
    REQUIRE(!captured0.matches(*make_function_id(lambda_add)));

    captured_id captured1(make_function_id(lambda_add));
    REQUIRE(captured1.matches(*make_function_id(lambda_add)));
    REQUIRE(!captured1.matches(*make_function_id(lambda_subtract)));
    REQUIRE(!captured1.matches(*make_function_id(add)));
}

namespace {

dynamic
dynamic_subtract(dynamic_array args, tasklet_tracker*)
{
    return dynamic(cast<double>(args.at(0)) - cast<double>(args.at(1)));
}

} // namespace

TEST_CASE("individual calcs", "[calcs][ws]")
{
    thinknode_test_scope scope{"", true};

    auto ctx{scope.make_context()};
    auto eval = [&](calculation_request const& request) {
        return cppcoro::sync_wait(resolve_calc_to_value(
            ctx, "5dadeb4a004073e81b5e096255e83652", request));
    };

    // value
    REQUIRE(
        eval(make_calculation_request_with_value(dynamic(2.5)))
        == dynamic(2.5));
    REQUIRE(
        eval(make_calculation_request_with_value(dynamic("foobar")))
        == dynamic("foobar"));
    REQUIRE(
        eval(make_calculation_request_with_value(
            {dynamic(1.0), dynamic(true), dynamic("x")}))
        == dynamic({dynamic(1.0), dynamic(true), dynamic("x")}));

    // reference
    REQUIRE(
        eval(make_calculation_request_with_reference(
            "5abd360900c0b14726b4ba1e6e5cdc12"))
        == dynamic(
            {{dynamic("demographics"),
              {
                  {dynamic("birthdate"),
                   {{dynamic("some"), dynamic("1800-01-01")}}},
                  {dynamic("sex"), {{dynamic("some"), dynamic("o")}}},
              }},
             {dynamic("medical_record_number"), dynamic("017-08-01")},
             {dynamic("name"),
              {{dynamic("family_name"), dynamic("Astroid")},
               {dynamic("given_name"), dynamic("v2")},
               {dynamic("middle_name"), dynamic("")},
               {dynamic("prefix"), dynamic("")},
               {dynamic("suffix"), dynamic("")}}}}));

#ifdef CRADLE_LOCAL_DOCKER_TESTING
    // function
    REQUIRE(
        eval(make_calculation_request_with_function(make_function_application(
            "mgh",
            "dosimetry",
            "addition",
            execution_host_selection::LOCAL,
            none,
            {make_calculation_request_with_value(dynamic(2.0)),
             make_calculation_request_with_value(dynamic(0.125))})))
        == dynamic(2.125));
#endif

    // lambda w/ actual lambda
    REQUIRE(
        eval(make_calculation_request_with_lambda(make_lambda_calculation(
            make_function([](dynamic_array args, tasklet_tracker*) {
                return dynamic(
                    cast<double>(args.at(0)) - cast<double>(args.at(1)));
            }),
            {make_calculation_request_with_value(dynamic(7.0)),
             make_calculation_request_with_value(dynamic(1.0))})))
        == dynamic(6.0));

    // lambda w/ function pointer
    REQUIRE(
        eval(make_calculation_request_with_lambda(make_lambda_calculation(
            make_function(dynamic_subtract),
            {make_calculation_request_with_value(dynamic(8.0)),
             make_calculation_request_with_value(dynamic(1.0))})))
        == dynamic(7.0));

    // array
    REQUIRE(
        eval(make_calculation_request_with_array(make_array_calc_request(
            {make_calculation_request_with_value(dynamic(2)),
             make_calculation_request_with_value(dynamic(0)),
             make_calculation_request_with_value(dynamic(3))},
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()))))
        == dynamic({dynamic(2), dynamic(0), dynamic(3)}));

    // item
    REQUIRE(
        eval(make_calculation_request_with_item(make_item_calc_request(
            make_calculation_request_with_value(
                dynamic({dynamic(2), dynamic(0), dynamic(3)})),
            make_calculation_request_with_value(dynamic(1)),
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()))))
        == dynamic(0));

    // object
    REQUIRE(
        eval(make_calculation_request_with_object(make_object_calc_request(
            {{"two", make_calculation_request_with_value(dynamic(2))},
             {"oh", make_calculation_request_with_value(dynamic(0))},
             {"three", make_calculation_request_with_value(dynamic(3))}},
            make_thinknode_type_info_with_structure_type(
                make_thinknode_structure_info(
                    {{"two",
                      make_thinknode_structure_field_info(
                          "the two",
                          some(false),
                          make_thinknode_type_info_with_integer_type(
                              make_thinknode_integer_type()))},
                     {"oh",
                      make_thinknode_structure_field_info(
                          "the oh",
                          some(false),
                          make_thinknode_type_info_with_integer_type(
                              make_thinknode_integer_type()))},
                     {"three",
                      make_thinknode_structure_field_info(
                          "the three",
                          some(false),
                          make_thinknode_type_info_with_integer_type(
                              make_thinknode_integer_type()))}})))))
        == dynamic(
            {{dynamic("two"), dynamic(2)},
             {dynamic("oh"), dynamic(0)},
             {dynamic("three"), dynamic(3)}}));

    // property
    REQUIRE(
        eval(make_calculation_request_with_property(make_property_calc_request(
            make_calculation_request_with_value(dynamic(
                {{dynamic("two"), dynamic(2)},
                 {dynamic("oh"), dynamic(0)},
                 {dynamic("three"), dynamic(3)}})),
            make_calculation_request_with_value(dynamic("oh")),
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()))))
        == dynamic(0));

    // let/variable
    REQUIRE(
        eval(make_calculation_request_with_let(make_let_calc_request(
            {{"x", make_calculation_request_with_value(dynamic(2))}},
            make_calculation_request_with_variable("x"))))
        == dynamic(2));

    // meta
    REQUIRE(
        eval(make_calculation_request_with_meta(make_meta_calc_request(
            make_calculation_request_with_value(
                dynamic({{dynamic("value"), dynamic(1)}})),
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()))))
        == dynamic(1));

    // cast
    REQUIRE(
        eval(make_calculation_request_with_cast(make_cast_calc_request(
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()),
            make_calculation_request_with_value(dynamic(0.0)))))
        == dynamic(0));
}

TEST_CASE("lambda calc caching", "[calcs][ws]")
{
    thinknode_test_scope scope;

    int call_count = 0;

    auto add = make_function([&](dynamic_array args, tasklet_tracker*) {
        ++call_count;
        return dynamic(cast<double>(args.at(0)) + cast<double>(args.at(1)));
    });

    auto ctx{scope.make_context()};
    auto eval = [&](calculation_request const& request) {
        return cppcoro::sync_wait(resolve_calc_to_value(
            ctx, "5dadeb4a004073e81b5e096255e83652", request));
    };

    REQUIRE(
        eval(make_calculation_request_with_lambda(make_lambda_calculation(
            add,
            {make_calculation_request_with_value(dynamic(1.0)),
             make_calculation_request_with_value(dynamic(1.0))})))
        == dynamic(2.0));
    REQUIRE(call_count == 1);

    REQUIRE(
        eval(make_calculation_request_with_lambda(make_lambda_calculation(
            add,
            {make_calculation_request_with_value(dynamic(1.0)),
             make_calculation_request_with_value(dynamic(2.0))})))
        == dynamic(3.0));
    REQUIRE(call_count == 2);

    REQUIRE(
        eval(make_calculation_request_with_lambda(make_lambda_calculation(
            add,
            {make_calculation_request_with_value(dynamic(1.0)),
             make_calculation_request_with_value(dynamic(1.0))})))
        == dynamic(2.0));
    REQUIRE(call_count == 2);
}

TEST_CASE("mixed calcs", "[calcs][ws]")
{
    thinknode_test_scope scope{"", true};

    auto ctx{scope.make_context()};
    auto eval = [&](calculation_request const& request) {
        return cppcoro::sync_wait(resolve_calc_to_value(
            ctx, "5dadeb4a004073e81b5e096255e83652", request));
    };

#ifdef CRADLE_LOCAL_DOCKER_TESTING
    auto local_calc
        = make_calculation_request_with_function(make_function_application(
            "mgh",
            "dosimetry",
            "addition",
            execution_host_selection::LOCAL,
            none,
            {make_calculation_request_with_value(dynamic(1.0)),
             make_calculation_request_with_value(dynamic(2.0))}));
#else
    auto local_calc = make_calculation_request_with_value(dynamic(3.0));
#endif

    auto remote_calc
        = make_calculation_request_with_function(make_function_application(
            "mgh",
            "dosimetry",
            "addition",
            execution_host_selection::THINKNODE,
            none,
            {make_calculation_request_with_value(dynamic(3.0)), local_calc}));

    auto lambda_calc
        = make_calculation_request_with_lambda(make_lambda_calculation(
            make_function([](dynamic_array args, tasklet_tracker*) {
                return dynamic(
                    cast<double>(args.at(0)) - cast<double>(args.at(1)));
            }),
            {make_calculation_request_with_value(dynamic(7.0)), remote_calc}));

    auto function_pointer_calc
        = make_calculation_request_with_lambda(make_lambda_calculation(
            make_function(dynamic_subtract),
            {make_calculation_request_with_value(dynamic(8.0)), lambda_calc}));

    REQUIRE(eval(function_pointer_calc) == dynamic(7.0));
}

TEST_CASE("Thinknode calc conversion", "[calcs][ws]")
{
    // value
    auto original_value
        = make_thinknode_calc_request_with_value(dynamic("xyz"));
    auto converted_value = make_calculation_request_with_value(dynamic("xyz"));
    REQUIRE(as_generic_calc(original_value) == converted_value);

    // reference
    auto original_reference = make_thinknode_calc_request_with_reference("a");
    auto converted_reference = make_calculation_request_with_reference("a");
    REQUIRE(as_generic_calc(original_reference) == converted_reference);

    // function
    REQUIRE(
        as_generic_calc(make_thinknode_calc_request_with_function(
            make_thinknode_function_application(
                "my_account",
                "my_name",
                "my_function",
                none,
                {original_value, original_reference})))
        == make_calculation_request_with_function(make_function_application(
            "my_account",
            "my_name",
            "my_function",
            execution_host_selection::THINKNODE,
            none,
            {converted_value, converted_reference})));

    // array
    auto item_schema
        = make_thinknode_type_info_with_string_type(thinknode_string_type());
    auto original_array
        = make_thinknode_calc_request_with_array(make_thinknode_array_calc(
            {original_value, original_reference}, item_schema));
    auto converted_array
        = make_calculation_request_with_array(make_array_calc_request(
            {converted_value, converted_reference}, item_schema));
    REQUIRE(as_generic_calc(original_array) == converted_array);

    // item
    auto original_item
        = make_thinknode_calc_request_with_item(make_thinknode_item_calc(
            original_array,
            make_thinknode_calc_request_with_value(dynamic(0)),
            item_schema));
    auto converted_item
        = make_calculation_request_with_item(make_item_calc_request(
            converted_array,
            make_calculation_request_with_value(dynamic(0)),
            item_schema));
    REQUIRE(as_generic_calc(original_item) == converted_item);

    // object
    auto object_schema = make_thinknode_type_info_with_structure_type(
        make_thinknode_structure_info({
            {"i", make_thinknode_structure_field_info("", none, item_schema)},
            {"j", make_thinknode_structure_field_info("", none, item_schema)},
        }));
    auto original_object
        = make_thinknode_calc_request_with_object(make_thinknode_object_calc(
            {{"i", original_value}, {"j", original_reference}},
            object_schema));
    auto converted_object
        = make_calculation_request_with_object(make_object_calc_request(
            {{"i", converted_value}, {"j", converted_reference}},
            object_schema));
    REQUIRE(as_generic_calc(original_object) == converted_object);

    // property
    auto original_property = make_thinknode_calc_request_with_property(
        make_thinknode_property_calc(
            original_object,
            make_thinknode_calc_request_with_value(dynamic("j")),
            item_schema));
    auto converted_property
        = make_calculation_request_with_property(make_property_calc_request(
            converted_object,
            make_calculation_request_with_value(dynamic("j")),
            item_schema));
    REQUIRE(as_generic_calc(original_property) == converted_property);

    // let
    REQUIRE(
        as_generic_calc(
            make_thinknode_calc_request_with_let(make_thinknode_let_calc(
                std::map<string, thinknode_calc_request>{
                    {"a", original_value}, {"b", original_reference}},
                original_value)))
        == make_calculation_request_with_let(make_let_calc_request(
            std::map<string, calculation_request>{
                {"a", converted_value}, {"b", converted_reference}},
            converted_value)));

    // variables
    REQUIRE(
        as_generic_calc(make_thinknode_calc_request_with_variable("a"))
        == make_calculation_request_with_variable("a"));

    // meta
    auto array_schema = make_thinknode_type_info_with_array_type(
        make_thinknode_array_info(item_schema, none));
    REQUIRE(
        as_generic_calc(make_thinknode_calc_request_with_meta(
            make_thinknode_meta_calc(original_array, array_schema)))
        == make_calculation_request_with_meta(
            make_meta_calc_request(converted_array, array_schema)));
}
