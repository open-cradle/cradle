#include <cradle/inner/core/id.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <unordered_map>

#include <catch2/catch.hpp>

#include "../../support/ids.h"
#include <cradle/inner/core/sha256_hash_id.h>

using namespace cradle;

TEST_CASE("sha256_hashed_id", "[id]")
{
    test_different_ids(
        make_sha256_hashed_id("token", 0), make_sha256_hashed_id("token", 1));
    unique_hasher hasher;
    auto id{make_sha256_hashed_id("token", 0)};
    id.update_hash(hasher);
    auto as_string{hasher.get_string()};
    REQUIRE(as_string.length() == 64);
    REQUIRE(std::all_of(as_string.begin(), as_string.end(), [](char c) {
        return std::isxdigit(c);
    }));
}

TEST_CASE("captured sha256_hashed_id", "[id]")
{
    auto captured = make_captured_sha256_hashed_id(std::string("xyz"), 87);
    auto made = make_sha256_hashed_id(std::string("xyz"), 87);
    REQUIRE(*captured == made);
}
