#include <cstring>
#include <filesystem>
#include <vector>

#include <catch2/catch.hpp>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/fs/utilities.h>

namespace cradle {

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

// Verify that hasher's result equals the reference, as defined by ref_hasher
void
verify_ref_result(unique_hasher& hasher, unique_hasher& ref_hasher)
{
    auto actual_string{hasher.get_string()};
    auto ref_string{ref_hasher.get_string()};
    REQUIRE(actual_string.size() == SHA256_DIGEST_LENGTH * 2);
    REQUIRE(actual_string != null_string);
    REQUIRE(actual_string == ref_string);

    auto actual_result{hasher.get_result()};
    auto ref_result{ref_hasher.get_result()};
    REQUIRE(memcmp(&actual_result, &null_result, sizeof(actual_result)) != 0);
    REQUIRE(memcmp(&actual_result, &ref_result, sizeof(actual_result)) == 0);
}

static char const ref_data[] = {0x01, 0x02, 0x03, 0x04};

// Verify that hasher's result equals the reference
void
verify_ref_result(unique_hasher& hasher)
{
    // Calculate a reference hash over the reference data (ref_data)
    unique_hasher ref_hasher;
    ref_hasher.encode_bytes(ref_data, sizeof(ref_data));
    verify_ref_result(hasher, ref_hasher);
}

// Verify that hasher's result differs from the reference
void
verify_non_ref_result(unique_hasher& hasher)
{
    // Calculate a reference hash over the reference data (ref_data)
    unique_hasher ref_hasher;
    ref_hasher.encode_bytes(ref_data, sizeof(ref_data));
    auto ref_string{ref_hasher.get_string()};
    auto ref_result{ref_hasher.get_result()};

    auto actual_string{hasher.get_string()};
    REQUIRE(actual_string.size() == SHA256_DIGEST_LENGTH * 2);
    REQUIRE(actual_string != null_string);
    REQUIRE(actual_string != ref_string);

    auto actual_result{hasher.get_result()};
    REQUIRE(memcmp(&actual_result, &null_result, sizeof(actual_result)) != 0);
    REQUIRE(memcmp(&actual_result, &ref_result, sizeof(actual_result)) != 0);
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

TEST_CASE("update_unique_hash: plain blob", tag)
{
    unique_hasher hasher;
    std::string ref_data_string{ref_data, sizeof(ref_data)};

    // Calculate a reference hash over the reference data:
    // a 0x00 tag, followed by the blob data
    std::string plain_blob_ref_data{'\x00'};
    plain_blob_ref_data.append(ref_data_string);
    unique_hasher ref_hasher;
    update_unique_hash(ref_hasher, plain_blob_ref_data);

    auto val{make_blob(ref_data_string)};
    update_unique_hash(hasher, val);
    verify_ref_result(hasher, ref_hasher);
}

TEST_CASE("update_unique_hash: blob file", tag)
{
    namespace fs = std::filesystem;
    std::string cache_dir{"tests_cache"};
    fs::path cache_dir_path{cache_dir};
    reset_directory(cache_dir_path);
    fs::path path{cache_dir_path / "blob_19"};

    // Calculate a reference hash over the reference data:
    // a 0x01 tag, followed by the path.
    std::string blob_file_ref_data{'\x01'};
    blob_file_ref_data.append(path.string());
    unique_hasher ref_hasher;
    update_unique_hash(ref_hasher, blob_file_ref_data);

    // Test a blob owned by a blob file writer.
    // Contents of the blob file don't matter for this test.
    auto shared_writer{std::make_shared<blob_file_writer>(path, 5)};
    auto& writer{*shared_writer};
    std::memcpy(writer.data(), "abcde", 5);
    writer.on_write_completed();
    blob writer_blob{shared_writer, writer.bytes(), writer.size()};
    unique_hasher writer_hasher;
    update_unique_hash(writer_hasher, writer_blob);
    verify_ref_result(writer_hasher, ref_hasher);

    // Test a blob owned by a blob file reader.
    auto shared_reader{std::make_shared<blob_file_reader>(path)};
    auto& reader{*shared_reader};
    blob reader_blob{shared_reader, reader.bytes(), reader.size()};
    unique_hasher reader_hasher;
    update_unique_hash(reader_hasher, reader_blob);
    verify_ref_result(reader_hasher, ref_hasher);
}

TEST_CASE("unique_hash: vector basic", tag)
{
    using my_vector_type = std::vector<int>;
    my_vector_type a{1, 2};
    my_vector_type b{1, 3};
    REQUIRE(get_unique_string_tmpl(a) != get_unique_string_tmpl(b));
}

// Illustrates why the hash of a vector should be based on more than just the
// hashes of its elements (but also on its size).
TEST_CASE("unique_hash: vector edge case", tag)
{
    using my_vector_type = std::vector<std::vector<int>>;
    my_vector_type a{{1, 2}, {3}};
    my_vector_type b{{1}, {2, 3}};
    REQUIRE(get_unique_string_tmpl(a) != get_unique_string_tmpl(b));
}

} // namespace cradle
