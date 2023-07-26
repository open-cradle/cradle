#include <cradle/websocket/local_calcs.h>

#include <thread>

#include <cppcoro/sync_wait.hpp>

#include <cradle/typing/utilities/testing.h>

#include <cradle/inner/encodings/base64.h>
#include <cradle/inner/utilities/environment.h>
#include <cradle/websocket/calculations.h>
#include <cradle/websocket/messages.hpp>

using namespace cradle;

#ifdef CRADLE_LOCAL_DOCKER_TESTING
TEST_CASE("local calcs", "[local_calcs][ws]")
{
    service_core core;
    init_test_service(core);

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token
        = get_environment_variable("CRADLE_THINKNODE_API_TOKEN");

    // These tests were originally written to test local resolution of
    // Thinknode calculations, which has been replaced by
    // resolve_calc_to_value. However, it's still a useful test to see if those
    // Thinknode calculations can be dynamically converted to the new generic
    // calculations and resolved to the same value.
    thinknode_request_context trc{core, session, nullptr, false, ""};
    auto eval = [&](thinknode_calc_request const& request) {
        return cppcoro::sync_wait(resolve_calc_to_value(
            trc,
            "5dadeb4a004073e81b5e096255e83652",
            from_dynamic<calculation_request>(to_dynamic(request))));
    };

    // value
    REQUIRE(
        eval(make_thinknode_calc_request_with_value(dynamic{2.5}))
        == dynamic{2.5});
    REQUIRE(
        eval(make_thinknode_calc_request_with_value(dynamic{"foobar"}))
        == dynamic{"foobar"});
    REQUIRE(
        eval(make_thinknode_calc_request_with_value(dynamic({1.0, true, "x"})))
        == dynamic({1.0, true, "x"}));

    // reference
    REQUIRE(
        eval(make_thinknode_calc_request_with_reference(
            "5abd360900c0b14726b4ba1e6e5cdc12"))
        == dynamic(
            {{"demographics",
              {
                  {"birthdate", {{"some", "1800-01-01"}}},
                  {"sex", {{"some", "o"}}},
              }},
             {"medical_record_number", "017-08-01"},
             {"name",
              {{"family_name", "Astroid"},
               {"given_name", "v2"},
               {"middle_name", ""},
               {"prefix", ""},
               {"suffix", ""}}}}));

    // function
    REQUIRE(
        eval(make_thinknode_calc_request_with_function(
            make_thinknode_function_application(
                "mgh",
                "dosimetry",
                "addition",
                none,
                {make_thinknode_calc_request_with_value(dynamic(2.0)),
                 make_thinknode_calc_request_with_value(dynamic(0.125))})))
        == dynamic(2.125));

    // array
    REQUIRE(
        eval(make_thinknode_calc_request_with_array(make_thinknode_array_calc(
            {make_thinknode_calc_request_with_value(dynamic(integer(2))),
             make_thinknode_calc_request_with_value(dynamic(integer(0))),
             make_thinknode_calc_request_with_value(dynamic(integer(3)))},
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()))))
        == dynamic({integer(2), integer(0), integer(3)}));

    // item
    REQUIRE(
        eval(make_thinknode_calc_request_with_item(make_thinknode_item_calc(
            make_thinknode_calc_request_with_value(
                dynamic({integer(2), integer(0), integer(3)})),
            make_thinknode_calc_request_with_value(dynamic(integer(1))),
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()))))
        == dynamic(integer(0)));

    // object
    REQUIRE(
        eval(
            make_thinknode_calc_request_with_object(make_thinknode_object_calc(
                {{"two",
                  make_thinknode_calc_request_with_value(dynamic(integer(2)))},
                 {"oh",
                  make_thinknode_calc_request_with_value(dynamic(integer(0)))},
                 {"three",
                  make_thinknode_calc_request_with_value(
                      dynamic(integer(3)))}},
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
            {{"two", integer(2)}, {"oh", integer(0)}, {"three", integer(3)}}));

    // property
    REQUIRE(
        eval(make_thinknode_calc_request_with_property(
            make_thinknode_property_calc(
                make_thinknode_calc_request_with_value(dynamic(
                    {{"two", integer(2)},
                     {"oh", integer(0)},
                     {"three", integer(3)}})),
                make_thinknode_calc_request_with_value(dynamic("oh")),
                make_thinknode_type_info_with_integer_type(
                    make_thinknode_integer_type()))))
        == dynamic(integer(0)));

    // let/variable
    REQUIRE(
        eval(make_thinknode_calc_request_with_let(make_thinknode_let_calc(
            {{"x",
              make_thinknode_calc_request_with_value(dynamic(integer(2)))}},
            make_thinknode_calc_request_with_variable("x"))))
        == dynamic(integer(2)));

    // meta
    REQUIRE(
        eval(make_thinknode_calc_request_with_meta(make_thinknode_meta_calc(
            make_thinknode_calc_request_with_value(
                dynamic({{"value", integer(1)}})),
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()))))
        == dynamic(integer(1)));

    // cast
    REQUIRE(
        eval(make_thinknode_calc_request_with_cast(make_thinknode_cast_request(
            make_thinknode_type_info_with_integer_type(
                make_thinknode_integer_type()),
            make_thinknode_calc_request_with_value(dynamic(0.0)))))
        == dynamic(integer(0)));
}
#endif
