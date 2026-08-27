#include <cradle/inner/storage/memory_ac.h>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <thread>
#include <vector>

#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/digest.h>

using namespace cradle;

static char const tag[] = "[unit][inner][storage][ac]";

TEST_CASE("memory_ac - put/get association", tag)
{
    auto ac = make_memory_ac("test_ac");
    request_key key = "request_key_1";
    digest value = "result_digest_abc123";

    // Put an association
    cppcoro::sync_wait(ac->put(key, value));

    // Get it back and verify it matches (FR-5, FR-6)
    auto retrieved = cppcoro::sync_wait(ac->get(key));
    REQUIRE(retrieved.has_value());
    REQUIRE(*retrieved == value);
}

TEST_CASE("memory_ac - miss returns nullopt", tag)
{
    auto ac = make_memory_ac("test_ac");
    request_key key = "never_associated_key";

    // Get on a never-associated request key should return nullopt (FR-7)
    auto retrieved = cppcoro::sync_wait(ac->get(key));
    REQUIRE_FALSE(retrieved.has_value());
}

TEST_CASE("memory_ac - exists", tag)
{
    auto ac = make_memory_ac("test_ac");
    request_key key = "request_key_exists_test";
    digest value = "result_digest_xyz789";

    // exists should be false before put (FR-8)
    bool exists_before = cppcoro::sync_wait(ac->exists(key));
    REQUIRE_FALSE(exists_before);

    // Put the association
    cppcoro::sync_wait(ac->put(key, value));

    // exists should be true after put (FR-8)
    bool exists_after = cppcoro::sync_wait(ac->exists(key));
    REQUIRE(exists_after);
}

TEST_CASE("memory_ac - idempotent put preserves original association", tag)
{
    auto ac = make_memory_ac("test_ac");
    request_key key = "request_key_idempotent";
    digest original_value = "original_digest_123";
    digest different_value = "different_digest_456";

    // First put stores the original association
    cppcoro::sync_wait(ac->put(key, original_value));

    // Second put with the same key but different digest should be a no-op
    cppcoro::sync_wait(ac->put(key, different_value));

    // Verify the original digest is still stored (idempotent behavior)
    auto retrieved = cppcoro::sync_wait(ac->get(key));
    REQUIRE(retrieved.has_value());
    REQUIRE(*retrieved == original_value);
}

TEST_CASE("memory_ac - multiple distinct associations", tag)
{
    auto ac = make_memory_ac("test_ac");
    request_key key1 = "request_key_1";
    request_key key2 = "request_key_2";
    digest value1 = "result_digest_1";
    digest value2 = "result_digest_2";

    // Put multiple distinct associations
    cppcoro::sync_wait(ac->put(key1, value1));
    cppcoro::sync_wait(ac->put(key2, value2));

    // Verify each association is independent
    auto retrieved1 = cppcoro::sync_wait(ac->get(key1));
    auto retrieved2 = cppcoro::sync_wait(ac->get(key2));
    REQUIRE(retrieved1.has_value());
    REQUIRE(*retrieved1 == value1);
    REQUIRE(retrieved2.has_value());
    REQUIRE(*retrieved2 == value2);
}

TEST_CASE("memory_ac - concurrent put operations", tag)
{
    auto ac = make_memory_ac("test_ac");
    request_key key1 = "concurrent_key_1";
    request_key key2 = "concurrent_key_2";
    request_key key3 = "concurrent_key_3";
    digest value1 = "concurrent_digest_1";
    digest value2 = "concurrent_digest_2";
    digest value3 = "concurrent_digest_3";

    // Launch multiple threads doing put of different keys concurrently (NFR-1)
    std::vector<std::thread> threads;
    threads.emplace_back([&]() {
        cppcoro::sync_wait(ac->put(key1, value1));
    });
    threads.emplace_back([&]() {
        cppcoro::sync_wait(ac->put(key2, value2));
    });
    threads.emplace_back([&]() {
        cppcoro::sync_wait(ac->put(key3, value3));
    });

    // Wait for all threads to complete
    for (auto& t : threads)
    {
        t.join();
    }

    // After concurrent puts, all keys should exist and have correct values
    bool exists1 = cppcoro::sync_wait(ac->exists(key1));
    bool exists2 = cppcoro::sync_wait(ac->exists(key2));
    bool exists3 = cppcoro::sync_wait(ac->exists(key3));
    REQUIRE(exists1);
    REQUIRE(exists2);
    REQUIRE(exists3);

    auto retrieved1 = cppcoro::sync_wait(ac->get(key1));
    auto retrieved2 = cppcoro::sync_wait(ac->get(key2));
    auto retrieved3 = cppcoro::sync_wait(ac->get(key3));
    REQUIRE(retrieved1.has_value());
    REQUIRE(*retrieved1 == value1);
    REQUIRE(retrieved2.has_value());
    REQUIRE(*retrieved2 == value2);
    REQUIRE(retrieved3.has_value());
    REQUIRE(*retrieved3 == value3);
}

TEST_CASE("memory_ac - concurrent same-key put race-freedom", tag)
{
    auto ac = make_memory_ac("test_ac");
    request_key key = "concurrent_key";
    digest value1 = "digest_from_thread_1";
    digest value2 = "digest_from_thread_2";
    digest value3 = "digest_from_thread_3";

    // Launch multiple threads doing put of the same key concurrently (NFR-1)
    std::vector<std::thread> threads;
    threads.emplace_back([&]() {
        cppcoro::sync_wait(ac->put(key, value1));
    });
    threads.emplace_back([&]() {
        cppcoro::sync_wait(ac->put(key, value2));
    });
    threads.emplace_back([&]() {
        cppcoro::sync_wait(ac->put(key, value3));
    });

    // Wait for all threads to complete
    for (auto& t : threads)
    {
        t.join();
    }

    // After concurrent puts, the key should exist
    bool exists = cppcoro::sync_wait(ac->exists(key));
    REQUIRE(exists);

    // Get should return one of the stored digests (whichever won the race)
    auto retrieved = cppcoro::sync_wait(ac->get(key));
    REQUIRE(retrieved.has_value());

    // The retrieved digest should be one of the three (proving no corruption)
    bool is_valid = (*retrieved == value1 || *retrieved == value2
                     || *retrieved == value3);
    REQUIRE(is_valid);
}

TEST_CASE("memory_ac - concurrent get/exists operations", tag)
{
    auto ac = make_memory_ac("test_ac");
    request_key key = "concurrent_read_key";
    digest value = "concurrent_read_digest";

    // Pre-populate the association
    cppcoro::sync_wait(ac->put(key, value));

    // Launch multiple threads doing concurrent reads (NFR-1)
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([&]() {
            auto retrieved = cppcoro::sync_wait(ac->get(key));
            REQUIRE(retrieved.has_value());
            REQUIRE(*retrieved == value);

            bool exists = cppcoro::sync_wait(ac->exists(key));
            REQUIRE(exists);
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads)
    {
        t.join();
    }

    // Final state should still be consistent
    auto retrieved = cppcoro::sync_wait(ac->get(key));
    REQUIRE(retrieved.has_value());
    REQUIRE(*retrieved == value);
}

TEST_CASE("memory_ac - name accessor", tag)
{
    std::string expected_name = "my_custom_ac_name";
    auto ac = make_memory_ac(expected_name);

    // Verify name() returns the factory-supplied name
    REQUIRE(ac->name() == expected_name);
}

TEST_CASE("memory_ac - default name when using default constructor", tag)
{
    memory_ac_impl ac;

    // Verify the default name is "memory_ac"
    REQUIRE(ac.name() == "memory_ac");
}
