#include <cradle/typing/encodings/yaml.h>

#include <cradle/inner/utilities/text.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

static string
strip_whitespace(string s)
{
    s.erase(std::remove_if(s.begin(), s.end(), isspace), s.end());
    return s;
}

// Test that a YAML string can be translated to its expected dynamic form.
// (But don't test the inverse.)
static void
test_one_way_yaml_encoding(
    string const& yaml, dynamic const& expected_value, bool round_trip = true)
{
    CAPTURE(yaml);

    // Parse it and check that it matches.
    auto converted_value = parse_yaml_value(yaml);
    REQUIRE(converted_value == expected_value);
}

// Test that a YAML string can be translated to and from its expected dynamic
// form.
static void
test_yaml_encoding(
    string const& yaml, dynamic const& expected_value, bool round_trip = true)
{
    CAPTURE(yaml);

    // Parse it and check that it matches.
    auto converted_value = parse_yaml_value(yaml);
    REQUIRE(converted_value == expected_value);

    // Convert it back to YAML and check that that matches the original (modulo
    // whitespace).
    auto converted_yaml = value_to_yaml(converted_value);
    REQUIRE(strip_whitespace(converted_yaml) == strip_whitespace(yaml));

    // Also try it as a blob.
    auto yaml_blob = value_to_yaml_blob(converted_value);
    REQUIRE(
        string(
            reinterpret_cast<char const*>(yaml_blob.data()),
            reinterpret_cast<char const*>(yaml_blob.data()) + yaml_blob.size())
        == converted_yaml);
}

// Test that dynamic value can be translated to the expected diagnostic
// encoding.
static void
test_diagnostic_yaml_encoding(
    dynamic const& value, string const& expected_yaml)
{
    auto yaml = value_to_diagnostic_yaml(value);
    REQUIRE(strip_whitespace(yaml) == strip_whitespace(expected_yaml));
}

TEST_CASE("basic YAML encoding", "[encodings][yaml]")
{
    // Try some basic types.
    test_yaml_encoding(
        R"(

        )",
        dynamic(nil));
    test_yaml_encoding(
        R"(
            false
        )",
        dynamic(false));
    test_yaml_encoding(
        R"(
            true
        )",
        dynamic(true));
    test_yaml_encoding(
        R"(
            "true"
        )",
        dynamic("true"));
    test_yaml_encoding(
        R"(
            1
        )",
        dynamic(integer(1)));
    test_yaml_encoding(
        R"(
            -1
        )",
        dynamic(integer(-1)));
    test_yaml_encoding(
        R"(
            1.25
        )",
        dynamic(1.25));
    test_yaml_encoding(
        R"(
            "1.25"
        )",
        dynamic("1.25"));
    test_one_way_yaml_encoding(
        R"(
            0x10
        )",
        dynamic(integer(16)));
    test_one_way_yaml_encoding(
        R"(
            0o10
        )",
        dynamic(integer(8)));
    test_one_way_yaml_encoding(
        R"(
            "hi"
        )",
        dynamic("hi"));

    // Try some arrays.
    test_yaml_encoding(
        R"(
            - 1
            - 2
            - 3
        )",
        {dynamic(1), dynamic(2), dynamic(3)});
    test_yaml_encoding(
        R"(
            []
        )",
        dynamic(dynamic_array()));

    // Try a map with string keys.
    test_yaml_encoding(
        R"(
            happy: true
            n: 4.125
        )",
        {{dynamic("happy"), dynamic(true)}, {dynamic("n"), dynamic(4.125)}});

    // Try a map with non-string keys.
    test_yaml_encoding(
        R"(
            false: 4.125
            0.125: xyz
        )",
        dynamic(dynamic_map{
            {dynamic(false), dynamic(4.125)},
            {dynamic(0.125), dynamic("xyz")}}));

    // Try some ptimes.
    test_yaml_encoding(
        R"(
            "2017-04-26T01:02:03.000Z"
        )",
        dynamic(ptime(
            date(2017, boost::gregorian::Apr, 26),
            boost::posix_time::time_duration(1, 2, 3))));
    test_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.456Z"
        )",
        dynamic(ptime(
            date(2017, boost::gregorian::May, 26),
            boost::posix_time::time_duration(13, 2, 3)
                + boost::posix_time::milliseconds(456))));

    // Try some thing that look like a ptime at first and check that they're
    // just treated as strings.
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:13:03.456ZABC"
        )",
        dynamic("2017-05-26T13:13:03.456ZABC"));
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:XX:03.456Z"
        )",
        dynamic("2017-05-26T13:XX:03.456Z"));
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:03.456Z"
        )",
        dynamic("2017-05-26T13:03.456Z"));
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T42:00:03.456Z"
        )",
        dynamic("2017-05-26T42:00:03.456Z"));
    test_one_way_yaml_encoding(
        R"(
            "X017-05-26T13:02:03.456Z"
        )",
        dynamic("X017-05-26T13:02:03.456Z"));
    test_one_way_yaml_encoding(
        R"(
            "2X17-05-26T13:02:03.456Z"
        )",
        dynamic("2X17-05-26T13:02:03.456Z"));
    test_one_way_yaml_encoding(
        R"(
            "20X7-05-26T13:02:03.456Z"
        )",
        dynamic("20X7-05-26T13:02:03.456Z"));
    test_one_way_yaml_encoding(
        R"(
            "201X-05-26T13:02:03.456Z"
        )",
        dynamic("201X-05-26T13:02:03.456Z"));
    test_one_way_yaml_encoding(
        R"(
            "2017X05-26T13:02:03.456Z"
        )",
        dynamic("2017X05-26T13:02:03.456Z"));
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.456_"
        )",
        dynamic("2017-05-26T13:02:03.456_"));
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.456_"
        )",
        dynamic("2017-05-26T13:02:03.456_"));
    test_one_way_yaml_encoding(
        R"(
            "2017-05-26T13:02:03.45Z"
        )",
        dynamic("2017-05-26T13:02:03.45Z"));

    // Try a blob.
    test_yaml_encoding(
        R"(
            type: base64-encoded-blob
            blob: c29tZSBibG9iIGRhdGE=
        )",
        dynamic(make_string_literal_blob("some blob data")));

    // Try some other things that aren't blobs but look similar.
    test_yaml_encoding(
        R"(
            blob: 1
            type: blob
        )",
        {{dynamic("type"), dynamic("blob")},
         {dynamic("blob"), dynamic(integer(1))}});
    test_yaml_encoding(
        R"(
            blob: awe
            type: 12
        )",
        {{dynamic("type"), dynamic(integer(12))},
         {dynamic("blob"), dynamic("awe")}});
}

TEST_CASE("diagnostic YAML encoding", "[encodings][yaml]")
{
    auto empty_blob = make_string_literal_blob("");
    test_diagnostic_yaml_encoding(dynamic(empty_blob), "0-bytes blob");

    auto small_blob = make_string_literal_blob("small blob");
    test_diagnostic_yaml_encoding(
        dynamic(small_blob), R"("10-bytes blob: smallblob")");

    byte_vector large_vector(16384);
    auto large_blob{make_blob(large_vector)};
    test_diagnostic_yaml_encoding(
        dynamic(large_blob),
        R"("16384-bytes blob: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ... 00 00 00 00")");

    auto unprintable_blob = make_string_literal_blob("\xf1wxyz");
    test_diagnostic_yaml_encoding(
        dynamic(unprintable_blob), R"("5-bytes blob: f1 77 78 79 7a")");

    test_diagnostic_yaml_encoding(
        dynamic(dynamic_map{
            {dynamic(false), dynamic(small_blob)},
            {dynamic(0.125), dynamic("xyz")}}),
        R"(
            false: "10-bytes blob: smallblob"
            0.125: xyz
        )");

    auto small_array = to_dynamic(std::vector<int>{1, 2, 3});
    test_diagnostic_yaml_encoding(small_array, "- 1\n- 2\n- 3\n");

    auto large_array = to_dynamic(std::vector<int>(100, 0));
    test_diagnostic_yaml_encoding(large_array, "\"<array - size: 100>\"");

    std::map<std::string, int> large_map;
    for (int i = 0; i != 100; ++i)
        large_map[std::to_string(i)] = i;
    test_diagnostic_yaml_encoding(
        to_dynamic(large_map), "\"<map - size: 100>\"");
}

TEST_CASE("malformed YAML blob", "[encodings][yaml]")
{
    try
    {
        parse_yaml_value(
            R"(
                {
                    type: base64-encoded-blob
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
                        type: base64-encoded-blob
                    }
                )"));
        REQUIRE(!get_required_error_info<parsing_error_info>(e).empty());
    }

    try
    {
        parse_yaml_value(
            R"(
                {
                    foo: 12,
                    bar: {
                        blob: 4,
                        type: base64-encoded-blob
                    }
                }
            )");
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(get_required_error_info<expected_format_info>(e) == "base64");
        REQUIRE(get_required_error_info<parsed_text_info>(e) == "4");
    }
}

static void
test_malformed_yaml(string const& malformed_yaml)
{
    CAPTURE(malformed_yaml);

    try
    {
        parse_yaml_value(malformed_yaml);
        FAIL("no exception thrown");
    }
    catch (parsing_error& e)
    {
        REQUIRE(get_required_error_info<expected_format_info>(e) == "YAML");
        REQUIRE(
            get_required_error_info<parsed_text_info>(e) == malformed_yaml);
        REQUIRE(!get_required_error_info<parsing_error_info>(e).empty());
    }
}

TEST_CASE("malformed YAML", "[encodings][yaml]")
{
    test_malformed_yaml(
        R"(
            ]asdf
        )");
    test_malformed_yaml(
        R"(
            asdf: [123
        )");
}
