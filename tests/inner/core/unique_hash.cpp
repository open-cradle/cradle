#include <cstring>

#include <catch2/catch.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/core/unique_hash.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][unique_hash]";

// The hash over an empty input sequence.
// Reference: sha256sum /dev/null
static unique_hasher::result_t const null_result{
    0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
    0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
    0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55,
};

static std::string const null_string{
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};

static char const ref_data[] = {0x01, 0x02, 0x03, 0x04};

// Get the hash result over the reference input (ref_data)
unique_hasher::result_t
get_ref_result()
{
    unique_hasher hasher;
    hasher.encode_bytes(ref_data, 4);
    return hasher.get_result();
}

// Get the hash string over the reference input (ref_data)
std::string
get_ref_string()
{
    unique_hasher hasher;
    hasher.encode_bytes(ref_data, 4);
    return hasher.get_string();
}

// Verify that hasher's result equals the reference
void
verify_ref_result(unique_hasher& hasher)
{
    auto result{hasher.get_result()};
    REQUIRE(memcmp(&result, &null_result, sizeof(result)) != 0);
    auto ref_result{get_ref_result()};
    REQUIRE(memcmp(&result, &ref_result, sizeof(result)) == 0);

    auto s{hasher.get_string()};
    REQUIRE(s.size() == SHA256_DIGEST_LENGTH * 2);
    REQUIRE(s != null_string);
    REQUIRE(s == get_ref_string());
}

// Verify that hasher's result differs from the reference
void
verify_non_ref_result(unique_hasher& hasher)
{
    auto result{hasher.get_result()};
    REQUIRE(memcmp(&result, &null_result, sizeof(result)) != 0);
    auto ref_result{get_ref_result()};
    REQUIRE(memcmp(&result, &ref_result, sizeof(result)) != 0);

    auto s{hasher.get_string()};
    REQUIRE(s.size() == SHA256_DIGEST_LENGTH * 2);
    REQUIRE(s != null_string);
    REQUIRE(s != get_ref_string());
}

} // namespace

TEST_CASE("unique_hash: empty input", tag)
{
    unique_hasher hasher;

    auto result{hasher.get_result()};
    REQUIRE(memcmp(&result, &null_result, sizeof(result)) == 0);
    REQUIRE(hasher.get_string() == null_string);
}

TEST_CASE("unique_hash: encode ptr+len", tag)
{
    unique_hasher hasher;
    char const data[] = {0x01, 0x02, 0x03, 0x04};
    hasher.encode_bytes(data, 4);

    verify_ref_result(hasher);
}

TEST_CASE("unique_hash: encode byte_t array", tag)
{
    unique_hasher hasher;
    unique_hasher::byte_t const data[] = {0x01, 0x02, 0x03, 0x04};
    hasher.encode_bytes(data, data + 4);

    verify_ref_result(hasher);
}

TEST_CASE("unique_hash: encode std::byte array", tag)
{
    unique_hasher hasher;
    std::byte const data[]
        = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}};
    hasher.encode_bytes(data, data + 4);

    verify_ref_result(hasher);
}

TEST_CASE("unique_hash: encode char array", tag)
{
    unique_hasher hasher;
    char const data[] = {0x01, 0x02, 0x03, 0x04};
    hasher.encode_bytes(data, data + 4);

    verify_ref_result(hasher);
}

TEST_CASE("unique_hash: combine", tag)
{
    unique_hasher hasher;
    char const data[] = {0x01, 0x02, 0x03, 0x04};
    hasher.encode_bytes(data, 4);
    unique_hasher hasher1;
    char const data1[] = {0x11, 0x12, 0x13, 0x14};
    hasher1.encode_bytes(data1, 4);
    auto result1{hasher1.get_result()};

    hasher.combine(result1);

    verify_non_ref_result(hasher);
}

TEST_CASE("update_unique_hash: char", tag)
{
    unique_hasher hasher;
    update_unique_hash(hasher, static_cast<char>(ref_data[0]));
    update_unique_hash(hasher, static_cast<char>(ref_data[1]));
    update_unique_hash(hasher, static_cast<char>(ref_data[2]));
    update_unique_hash(hasher, static_cast<char>(ref_data[3]));

    verify_ref_result(hasher);
}

TEST_CASE("update_unique_hash: int", tag)
{
    unique_hasher hasher;
    update_unique_hash(hasher, static_cast<int>(1234));

    verify_non_ref_result(hasher);
}

TEST_CASE("update_unique_hash: float", tag)
{
    unique_hasher hasher;
    update_unique_hash(hasher, static_cast<float>(1.23));

    verify_non_ref_result(hasher);
}

TEST_CASE("update_unique_hash: double", tag)
{
    unique_hasher hasher;
    update_unique_hash(hasher, static_cast<double>(1.23));

    verify_non_ref_result(hasher);
}

TEST_CASE("update_unique_hash: string", tag)
{
    // Assumes that a string is hashed by hashing its characters
    unique_hasher hasher;
    auto val{std::string{ref_data, 4}};
    update_unique_hash(hasher, val);

    verify_ref_result(hasher);
}

TEST_CASE("update_unique_hash: blob", tag)
{
    // Assumes that a blob is hashed by hashing its bytes
    unique_hasher hasher;
    auto val{make_blob(std::string{ref_data, 4})};
    update_unique_hash(hasher, val);

    verify_ref_result(hasher);
}
