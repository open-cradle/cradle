#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

#include <catch2/catch.hpp>
#include <cereal/archives/json.hpp>
#include <fmt/format.h>

#include "../../support/inner_service.h"
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/cereal.h>
#include <cradle/inner/service/resources.h>

using namespace cradle;

static std::string
strip(std::string s)
{
    s.erase(std::remove_if(s.begin(), s.end(), isspace), s.end());
    return s;
}

static std::string
json_string(std::string const& s)
{
    std::string res;
    for (auto c : s)
    {
        if (c == '\\')
        {
            res += "\\\\";
        }
        else
        {
            res += c;
        }
    }
    return res;
}

static std::string
serialize_to_json(blob const& x)
{
    std::stringstream os;
    cereal::JSONOutputArchive oarchive(os);
    oarchive(x);
    // TODO workaround for bug in cereal
    return os.str() + "}";
}

static blob
deserialize_from_json(std::string const& json)
{
    std::istringstream is(json);
    cereal::JSONInputArchive iarchive(is);
    blob y;
    iarchive(y);
    return y;
}

static std::string
serialize_to_binary(blob const& x)
{
    std::stringstream os;
    cereal::BinaryOutputArchive oarchive(os);
    oarchive(x);
    return os.str();
}

static blob
deserialize_from_binary(std::string const& binary)
{
    std::istringstream is(binary);
    cereal::BinaryInputArchive iarchive(is);
    blob y;
    iarchive(y);
    return y;
}

static void
test_json(blob const& x, std::string const& expected)
{
    std::string serialized = serialize_to_json(x);
    REQUIRE(strip(serialized) == strip(expected));
    blob y = deserialize_from_json(serialized);
    REQUIRE(y == x);
}

static void
test_binary(blob const& x)
{
    std::string serialized = serialize_to_binary(x);
    blob y = deserialize_from_binary(serialized);
    REQUIRE(y == x);
}

static void
test_all(blob const& x, std::string const& expected_json)
{
    test_json(x, expected_json);
    test_binary(x);
}

TEST_CASE("cereal converting empty blob", "[encodings][cereal]")
{
    blob x;
    test_all(x, R"(
    {
        "value0": {
            "as_file": false,
            "size": 0,
            "blob": ""
        }
    }
    )");
}

TEST_CASE("cereal converting plain blob", "[encodings][cereal]")
{
    auto x{make_string_literal_blob("abcde")};
    test_all(x, R"(
    {
        "value0": {
            "as_file": false,
            "size": 5,
            "blob": "YWJjZGU="
        }
    }
    )");
}

TEST_CASE("cereal converting file blob", "[encodings][cereal]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto shared_writer{resources.make_blob_file_writer(3)};
    auto& writer{*shared_writer};
    std::memcpy(writer.data(), "fgh", 3);
    writer.on_write_completed();
    blob x{shared_writer, writer.bytes(), writer.size()};

    test_all(
        x,
        fmt::format(
            R"(
    {{
        "value0": {{
            "as_file": true,
            "path": "{}"
        }}
    }}
    )",
            json_string(writer.mapped_file())));
}
