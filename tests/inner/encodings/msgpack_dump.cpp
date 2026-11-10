#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch.hpp>
#include <msgpack.hpp>

#include <cradle/inner/encodings/msgpack_dump.h>

using namespace cradle;

namespace {

template<typename T>
std::string
dump_packed(T const& value)
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, value);
    msgpack::object_handle oh = msgpack::unpack(sbuf.data(), sbuf.size());
    std::ostringstream oss;
    dump_msgpack_object(oss, oh.get());
    return oss.str();
}

} // namespace

TEST_CASE("dump_msgpack_object positive integer", "[encodings][msgpack_dump]")
{
    REQUIRE(dump_packed(std::uint64_t{42}) == "POSITIVE_INTEGER 42\n");
}

TEST_CASE("dump_msgpack_object string", "[encodings][msgpack_dump]")
{
    REQUIRE(dump_packed(std::string{"foo"}) == "STR size 3 \"foo\"\n");
}

TEST_CASE("dump_msgpack_object array", "[encodings][msgpack_dump]")
{
    std::vector<std::uint64_t> values{1, 2};
    REQUIRE(
        dump_packed(values)
        == "ARRAY size 2\n"
           "    [0] POSITIVE_INTEGER 1\n"
           "    [1] POSITIVE_INTEGER 2\n");
}

TEST_CASE(
    "dump_msgpack_object nested array indentation",
    "[encodings][msgpack_dump]")
{
    std::vector<std::vector<std::uint64_t>> values{{7}};
    REQUIRE(
        dump_packed(values)
        == "ARRAY size 1\n"
           "    [0] ARRAY size 1\n"
           "        [0] POSITIVE_INTEGER 7\n");
}

TEST_CASE(
    "dump_msgpack_object unknown type is best-effort",
    "[encodings][msgpack_dump]")
{
    // This uses an intentionally invalid type code to test that the function
    // still produces an output for an unknown type.
    msgpack::object obj{};
    obj.type = static_cast<msgpack::type::object_type>(255);
    std::ostringstream oss;
    dump_msgpack_object(oss, obj);
    REQUIRE(oss.str() == "UNKNOWN type 255\n");
}
