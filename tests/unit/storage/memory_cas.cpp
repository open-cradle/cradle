#include <cradle/inner/storage/memory_cas.h>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <thread>
#include <vector>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/digest.h>

using namespace cradle;

static char const tag[] = "[unit][inner][storage][cas]";

TEST_CASE("memory_cas - idempotent put", tag)
{
    auto cas = make_memory_cas("test_cas");
    digest key = "digest_abc123";
    blob original_content = make_blob("original content");
    blob different_content = make_blob("different content");

    // First put stores the original content
    cppcoro::sync_wait(cas->put(key, original_content));

    // Second put with the same digest but different content should be a no-op
    cppcoro::sync_wait(cas->put(key, different_content));

    // Verify the original content is still stored (idempotent behavior)
    auto retrieved = cppcoro::sync_wait(cas->get(key));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "original content");
}

TEST_CASE("memory_cas - get round-trip", tag)
{
    auto cas = make_memory_cas("test_cas");
    digest key = "digest_xyz789";
    blob content = make_blob("test data for round-trip");

    // Put the content
    cppcoro::sync_wait(cas->put(key, content));

    // Get it back and verify it matches
    auto retrieved = cppcoro::sync_wait(cas->get(key));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "test data for round-trip");
}

TEST_CASE("memory_cas - miss returns nullopt", tag)
{
    auto cas = make_memory_cas("test_cas");
    digest key = "never_stored_digest";

    // Get on a never-stored digest should return nullopt
    auto retrieved = cppcoro::sync_wait(cas->get(key));
    REQUIRE_FALSE(retrieved.has_value());
}

TEST_CASE("memory_cas - exists", tag)
{
    auto cas = make_memory_cas("test_cas");
    digest key = "digest_exists_test";
    blob content = make_blob("some content");

    // exists should be false before put
    bool exists_before = cppcoro::sync_wait(cas->exists(key));
    REQUIRE_FALSE(exists_before);

    // Put the content
    cppcoro::sync_wait(cas->put(key, content));

    // exists should be true after put
    bool exists_after = cppcoro::sync_wait(cas->exists(key));
    REQUIRE(exists_after);
}

TEST_CASE("memory_cas - empty blob is a hit distinct from miss", tag)
{
    auto cas = make_memory_cas("test_cas");
    digest key_empty = "digest_empty";
    digest key_missing = "digest_missing";
    blob empty_content = make_blob("");

    // Store an empty blob
    cppcoro::sync_wait(cas->put(key_empty, empty_content));

    // Get the empty blob - should return a populated optional (a hit)
    auto retrieved_empty = cppcoro::sync_wait(cas->get(key_empty));
    REQUIRE(retrieved_empty.has_value());
    REQUIRE(retrieved_empty->size() == 0);

    // exists should return true for the empty blob
    bool exists_empty = cppcoro::sync_wait(cas->exists(key_empty));
    REQUIRE(exists_empty);

    // Get on a missing digest - should return nullopt (a miss)
    auto retrieved_missing = cppcoro::sync_wait(cas->get(key_missing));
    REQUIRE_FALSE(retrieved_missing.has_value());

    // exists should return false for the missing digest
    bool exists_missing = cppcoro::sync_wait(cas->exists(key_missing));
    REQUIRE_FALSE(exists_missing);
}

TEST_CASE("memory_cas - concurrent same-digest put race-freedom", tag)
{
    auto cas = make_memory_cas("test_cas");
    digest key = "concurrent_digest";
    blob content1 = make_blob("content from thread 1");
    blob content2 = make_blob("content from thread 2");
    blob content3 = make_blob("content from thread 3");

    // Launch multiple threads doing put of the same digest concurrently
    std::vector<std::thread> threads;
    threads.emplace_back([&]() {
        cppcoro::sync_wait(cas->put(key, content1));
    });
    threads.emplace_back([&]() {
        cppcoro::sync_wait(cas->put(key, content2));
    });
    threads.emplace_back([&]() {
        cppcoro::sync_wait(cas->put(key, content3));
    });

    // Wait for all threads to complete
    for (auto& t : threads)
    {
        t.join();
    }

    // After concurrent puts, the digest should exist
    bool exists = cppcoro::sync_wait(cas->exists(key));
    REQUIRE(exists);

    // Get should return one of the stored contents (whichever won the race)
    auto retrieved = cppcoro::sync_wait(cas->get(key));
    REQUIRE(retrieved.has_value());

    // The retrieved content should be one of the three (proving no corruption)
    std::string retrieved_str = to_string(*retrieved);
    bool is_valid = (retrieved_str == "content from thread 1"
                     || retrieved_str == "content from thread 2"
                     || retrieved_str == "content from thread 3");
    REQUIRE(is_valid);
}

TEST_CASE("memory_cas - concurrent get/exists operations", tag)
{
    auto cas = make_memory_cas("test_cas");
    digest key = "concurrent_read_digest";
    blob content = make_blob("concurrent read test content");

    // Store the content first
    cppcoro::sync_wait(cas->put(key, content));

    // Launch multiple threads doing concurrent get and exists
    std::atomic<int> successful_gets{0};
    std::atomic<int> successful_exists{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i)
    {
        threads.emplace_back([&]() {
            auto retrieved = cppcoro::sync_wait(cas->get(key));
            if (retrieved.has_value()
                && to_string(*retrieved) == "concurrent read test content")
            {
                successful_gets++;
            }
        });
        threads.emplace_back([&]() {
            bool exists = cppcoro::sync_wait(cas->exists(key));
            if (exists)
            {
                successful_exists++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads)
    {
        t.join();
    }

    // All reads should have succeeded without corruption
    REQUIRE(successful_gets == 5);
    REQUIRE(successful_exists == 5);
}

TEST_CASE("memory_cas - name accessor", tag)
{
    auto cas = make_memory_cas("custom_test_name");
    REQUIRE(cas->name() == "custom_test_name");
}
