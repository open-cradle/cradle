#include <cradle/typing/encodings/json.h>

#include <cradle/inner/utilities/text.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

static string
strip_whitespace(string s)
{
    s.erase(std::remove_if(s.begin(), s.end(), isspace), s.end());
    return s;
}

// Test that a JSON string can be translated to and from its expected dynamic
// form.
static void
test_json_encoding(string const& json, dynamic const& expected_value)
{
    CAPTURE(json);

    // Parse it and check that it matches.
    auto converted_value = parse_json_value(json);
    REQUIRE(converted_value == expected_value);

    // Convert it back to JSON and check that that matches the original (modulo
    // whitespace).
    auto converted_json = value_to_json(converted_value);
    REQUIRE(strip_whitespace(converted_json) == strip_whitespace(json));

    // Also try it as a blob.
    auto json_blob = value_to_json_blob(converted_value);
    REQUIRE(
        string(
            reinterpret_cast<char const*>(json_blob.data()),
            reinterpret_cast<char const*>(json_blob.data()) + json_blob.size())
        == converted_json);
}

TEST_CASE("basic JSON encoding", "[encodings][json]")
{
    // Try some basic types.
    test_json_encoding(
        R"(
            null
        )",
        dynamic{nil});
    test_json_encoding(
        R"(
            false
        )",
        dynamic{false});
    test_json_encoding(
        R"(
            true
        )",
        dynamic{true});
    test_json_encoding(
        R"(
            1
        )",
        dynamic{integer(1)});
    test_json_encoding(
        R"(
            10737418240
        )",
        dynamic{integer(10737418240)});
    test_json_encoding(
        R"(
            -1
        )",
        dynamic{integer(-1)});
    test_json_encoding(
        R"(
            1.25
        )",
        dynamic{1.25});
    test_json_encoding(
        R"(
            "hi"
        )",
        dynamic{"hi"});

    // Try some arrays.
    test_json_encoding(
        R"(
            [ 1, 2, 3 ]
        )",
        {dynamic{integer(1)}, dynamic{integer(2)}, dynamic{integer(3)}});
    test_json_encoding(
        R"(
            []
        )",
        dynamic{dynamic_array{}});

    // Try a map with string keys.
    test_json_encoding(
        R"(
            {
                "happy": true,
                "n": 4.125
            }
        )",
        {{dynamic{"happy"}, dynamic{true}}, {dynamic{"n"}, dynamic{4.125}}});

    // Try a map with non-string keys.
    test_json_encoding(
        R"(
            [
                {
                    "key": false,
                    "value": "no"
                },
                {
                    "key": true,
                    "value": "yes"
                }
            ]
        )",
        dynamic{dynamic_map{
            {dynamic{false}, dynamic{"no"}},
            {dynamic{true}, dynamic{"yes"}}}});

    // Try some other JSON that looks like the above.
    test_json_encoding(
        R"(
            [
                {
                    "key": false
                },
                {
                    "key": true
                }
            ]
        )",
        {{{dynamic{"key"}, dynamic{false}}},
         {{dynamic{"key"}, dynamic{true}}}});
    test_json_encoding(
        R"(
            [
                {
                    "key": false,
                    "valu": "no"
                },
                {
                    "key": true,
                    "valu": "yes"
                }
            ]
        )",
        {{{dynamic{"key"}, dynamic{false}}, {dynamic{"valu"}, dynamic{"no"}}},
         {{dynamic{"key"}, dynamic{true}},
          {dynamic{"valu"}, dynamic{"yes"}}}});
    test_json_encoding(
        R"(
            [
                {
                    "ke": false,
                    "value": "no"
                },
                {
                    "ke": true,
                    "value": "yes"
                }
            ]
        )",
        {{{dynamic{"ke"}, dynamic{false}}, {dynamic{"value"}, dynamic{"no"}}},
         {{dynamic{"ke"}, dynamic{true}},
          {dynamic{"value"}, dynamic{"yes"}}}});

    // Try some ptimes.
    test_json_encoding(
        R"(
            "2017-04-26T01:02:03.000Z"
        )",
        dynamic{ptime(
            date(2017, boost::gregorian::Apr, 26),
            boost::posix_time::time_duration(1, 2, 3))});
    test_json_encoding(
        R"(
            "2017-05-26T13:02:03.456Z"
        )",
        dynamic{ptime(
            date(2017, boost::gregorian::May, 26),
            boost::posix_time::time_duration(13, 2, 3)
                + boost::posix_time::milliseconds(456))});

    // Try some thing that look like a ptime at first and check that they're
    // just treated as strings.
    test_json_encoding(
        R"(
            "2017-05-26T13:13:03.456ZABC"
        )",
        dynamic{"2017-05-26T13:13:03.456ZABC"});
    test_json_encoding(
        R"(
            "2017-05-26T13:XX:03.456Z"
        )",
        dynamic{"2017-05-26T13:XX:03.456Z"});
    test_json_encoding(
        R"(
            "2017-05-26T13:03.456Z"
        )",
        dynamic{"2017-05-26T13:03.456Z"});
    test_json_encoding(
        R"(
            "2017-05-26T42:00:03.456Z"
        )",
        dynamic{"2017-05-26T42:00:03.456Z"});
    test_json_encoding(
        R"(
            "X017-05-26T13:02:03.456Z"
        )",
        dynamic{"X017-05-26T13:02:03.456Z"});
    test_json_encoding(
        R"(
            "2X17-05-26T13:02:03.456Z"
        )",
        dynamic{"2X17-05-26T13:02:03.456Z"});
    test_json_encoding(
        R"(
            "20X7-05-26T13:02:03.456Z"
        )",
        dynamic{"20X7-05-26T13:02:03.456Z"});
    test_json_encoding(
        R"(
            "201X-05-26T13:02:03.456Z"
        )",
        dynamic{"201X-05-26T13:02:03.456Z"});
    test_json_encoding(
        R"(
            "2017X05-26T13:02:03.456Z"
        )",
        dynamic{"2017X05-26T13:02:03.456Z"});
    test_json_encoding(
        R"(
            "2017-05-26T13:02:03.456_"
        )",
        dynamic{"2017-05-26T13:02:03.456_"});
    test_json_encoding(
        R"(
            "2017-05-26T13:02:03.456_"
        )",
        dynamic{"2017-05-26T13:02:03.456_"});
    test_json_encoding(
        R"(
            "2017-05-26T13:02:03.45Z"
        )",
        dynamic{"2017-05-26T13:02:03.45Z"});

    // Try a blob.
    test_json_encoding(
        R"(
            {
                "blob": "c29tZSBibG9iIGRhdGE=",
                "type": "base64-encoded-blob"
            }
        )",
        dynamic{make_string_literal_blob("some blob data")});

    // Try some other things that aren't blobs but look similar.
    test_json_encoding(
        R"(
            {
                "blob": "asdf",
                "type": "blob"
            }
        )",
        {{dynamic{"type"}, dynamic{"blob"}},
         {dynamic{"blob"}, dynamic{"asdf"}}});
    test_json_encoding(
        R"(
            {
                "blob": "asdf",
                "type": 12
            }
        )",
        {{dynamic{"type"}, dynamic{integer(12)}},
         {dynamic{"blob"}, dynamic{"asdf"}}});
}

TEST_CASE("malformed JSON blob", "[encodings][json]")
{
    try
    {
        parse_json_value(
            R"(
                {
                    "type": "base64-encoded-blob"
                }
            )");
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(
            get_required_error_info<expected_format_info>(e)
            == "base64-encoded-blob");
        REQUIRE(
            strip_whitespace(get_required_error_info<parsed_text_info>(e))
            == strip_whitespace(
                R"(
                    {
                        "type": "base64-encoded-blob"
                    }
                )"));
        REQUIRE(!get_required_error_info<parsing_error_info>(e).empty());
    }

    try
    {
        parse_json_value(
            R"(
                {
                    "foo": 12,
                    "bar": {
                        "blob": 4,
                        "type": "base64-encoded-blob"
                    }
                }
            )");
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(
            get_required_error_info<expected_format_info>(e)
            == "base64-encoded-blob");
        REQUIRE(
            strip_whitespace(get_required_error_info<parsed_text_info>(e))
            == strip_whitespace(
                R"(
                    {
                        "blob": 4,
                        "type": "base64-encoded-blob"
                    }
                )"));
        REQUIRE(!get_required_error_info<parsing_error_info>(e).empty());
    }
}

static void
test_malformed_json(string const& malformed_json)
{
    CAPTURE(malformed_json);

    try
    {
        parse_json_value(malformed_json);
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(get_required_error_info<expected_format_info>(e) == "JSON");
        REQUIRE(
            get_required_error_info<parsed_text_info>(e) == malformed_json);
        REQUIRE(!get_required_error_info<parsing_error_info>(e).empty());
    }
}

TEST_CASE("malformed JSON", "[encodings][json]")
{
    test_malformed_json(
        R"(
            asdf
        )");
    test_malformed_json(
        R"(
            asdf: 123
        )");
}
