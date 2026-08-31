#include <cradle/inner/storage/content_helpers.h>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/memory_cas.h>
#include <cradle/inner/utilities/errors.h>

using namespace cradle;

static char const tag[] = "[unit][inner][storage][content_helpers]";

TEST_CASE("digest_of - equal content yields equal digest", tag)
{
    blob content1 = make_blob("test content");
    blob content2 = make_blob("test content");

    digest d1 = digest_of(content1);
    digest d2 = digest_of(content2);

    // Equal content must yield equal digest
    REQUIRE(d1 == d2);
}

TEST_CASE("digest_of - different content yields different digest", tag)
{
    blob content1 = make_blob("test content");
    blob content2 = make_blob("different content");

    digest d1 = digest_of(content1);
    digest d2 = digest_of(content2);

    // Different content must yield different digest
    REQUIRE(d1 != d2);
}

TEST_CASE("digest_of - callable without storing", tag)
{
    blob content = make_blob("standalone hashing");

    // digest_of is a pure function; no CAS required
    digest d = digest_of(content);

    // Verify the digest is non-empty (a well-defined value was computed)
    REQUIRE_FALSE(d.empty());
}

TEST_CASE("digest_of - empty content has stable digest", tag)
{
    blob empty1 = make_blob("");
    blob empty2 = make_blob("");

    digest d1 = digest_of(empty1);
    digest d2 = digest_of(empty2);

    // Empty content should have a stable, non-empty digest
    REQUIRE_FALSE(d1.empty());
    REQUIRE(d1 == d2);
}

TEST_CASE("put_content - stores and returns digest", tag)
{
    auto cas = make_memory_cas("test_cas");
    blob content = make_blob("content to store");

    // put_content returns the digest
    digest d = cppcoro::sync_wait(put_content(*cas, content));

    // Verify the digest is non-empty
    REQUIRE_FALSE(d.empty());

    // Verify the content is now stored under that digest
    auto retrieved = cppcoro::sync_wait(cas->get(d));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "content to store");
}

TEST_CASE("put_content - idempotent behavior", tag)
{
    auto cas = make_memory_cas("test_cas");
    blob content = make_blob("idempotent content");

    // First put
    digest d1 = cppcoro::sync_wait(put_content(*cas, content));

    // Second put with same content
    digest d2 = cppcoro::sync_wait(put_content(*cas, content));

    // Both puts return the same digest
    REQUIRE(d1 == d2);

    // Verify the stored value is unchanged
    auto retrieved = cppcoro::sync_wait(cas->get(d1));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "idempotent content");
}

TEST_CASE("put_content - empty blob round-trip", tag)
{
    auto cas = make_memory_cas("test_cas");
    blob empty_content = make_blob("");

    // put_content with empty blob
    digest d = cppcoro::sync_wait(put_content(*cas, empty_content));

    // Verify a digest was returned
    REQUIRE_FALSE(d.empty());

    // Verify get returns a hit (not a miss) with empty payload
    auto retrieved = cppcoro::sync_wait(cas->get(d));
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->size() == 0);
}

TEST_CASE("put_content_if_absent - stores when absent", tag)
{
    auto cas = make_memory_cas("test_cas");
    blob content = make_blob("initially absent content");
    digest key = "test_digest_key";

    // Verify the key is absent
    bool exists_before = cppcoro::sync_wait(cas->exists(key));
    REQUIRE_FALSE(exists_before);

    // put_content_if_absent should store when absent
    cppcoro::sync_wait(put_content_if_absent(*cas, key, content));

    // Verify the content is now stored
    auto retrieved = cppcoro::sync_wait(cas->get(key));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "initially absent content");
}

TEST_CASE("put_content_if_absent - no-op when present", tag)
{
    auto cas = make_memory_cas("test_cas");
    digest key = "existing_digest";
    blob original_content = make_blob("original content");
    blob different_content = make_blob("different content");

    // Store original content under the digest
    cppcoro::sync_wait(cas->put(key, original_content));

    // put_content_if_absent with different content should be a no-op
    cppcoro::sync_wait(put_content_if_absent(*cas, key, different_content));

    // Verify the stored value is still the original (no overwrite)
    auto retrieved = cppcoro::sync_wait(cas->get(key));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "original content");
}

TEST_CASE("put_content_if_absent - uses caller-supplied digest", tag)
{
    auto cas = make_memory_cas("test_cas");
    blob content = make_blob("content with caller digest");
    // Arbitrary caller-chosen key (not the actual hash of content)
    digest caller_key = "arbitrary_caller_chosen_key";

    // put_content_if_absent uses the caller-supplied digest as-is
    cppcoro::sync_wait(put_content_if_absent(*cas, caller_key, content));

    // Verify the content is stored under the caller's key
    auto retrieved = cppcoro::sync_wait(cas->get(caller_key));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "content with caller digest");

    // Verify the CAS did not recompute the digest - the content is NOT
    // stored under the actual hash
    digest actual_hash = digest_of(content);
    REQUIRE(caller_key != actual_hash);
    auto not_under_hash = cppcoro::sync_wait(cas->get(actual_hash));
    REQUIRE_FALSE(not_under_hash.has_value());
}

TEST_CASE("put_content_verified - stores on match", tag)
{
    auto cas = make_memory_cas("test_cas");
    blob content = make_blob("verified content");
    digest expected = digest_of(content);

    // put_content_verified with correct digest should store
    cppcoro::sync_wait(put_content_verified(*cas, content, expected));

    // Verify the content is stored under the expected digest
    auto retrieved = cppcoro::sync_wait(cas->get(expected));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "verified content");
}

TEST_CASE("put_content_verified - throws on mismatch", tag)
{
    auto cas = make_memory_cas("test_cas");
    blob content = make_blob("content to verify");
    digest wrong_digest = "wrong_digest_value";

    // Verify the wrong digest is different from the actual digest
    digest actual_digest = digest_of(content);
    REQUIRE(wrong_digest != actual_digest);

    // put_content_verified with wrong digest should throw
    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(put_content_verified(*cas, content, wrong_digest)),
        internal_check_failed);

    // Verify the content was NOT stored under the wrong digest
    auto not_stored = cppcoro::sync_wait(cas->get(wrong_digest));
    REQUIRE_FALSE(not_stored.has_value());
}

TEST_CASE("put_content_verified - throws on mismatch, does not store", tag)
{
    auto cas = make_memory_cas("test_cas");
    blob content = make_blob("another content");
    digest wrong_digest = "another_wrong_digest";

    // Verify the digest is initially absent
    bool exists_before = cppcoro::sync_wait(cas->exists(wrong_digest));
    REQUIRE_FALSE(exists_before);

    // put_content_verified with wrong digest should throw
    REQUIRE_THROWS(
        cppcoro::sync_wait(put_content_verified(*cas, content, wrong_digest)));

    // Verify the digest is still absent (nothing was stored)
    bool exists_after = cppcoro::sync_wait(cas->exists(wrong_digest));
    REQUIRE_FALSE(exists_after);
}
