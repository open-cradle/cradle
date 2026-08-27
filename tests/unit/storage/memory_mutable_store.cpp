#include <cradle/inner/storage/memory_mutable_store.h>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <thread>
#include <vector>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/mutable_store_intf.h>

using namespace cradle;

static char const tag[] = "[unit][inner][storage][mutable_store]";

TEST_CASE("memory_mutable_store - put/get", tag)
{
    auto store = make_memory_mutable_store("test_store");
    std::string key = "my_key";
    mutable_value value = make_blob("my value content");

    // Put a value (FR-14)
    cppcoro::sync_wait(store->put(key, value));

    // Get it back and verify it matches (FR-15)
    auto retrieved = cppcoro::sync_wait(store->get(key));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "my value content");
}

TEST_CASE("memory_mutable_store - overwrite replaces prior value", tag)
{
    auto store = make_memory_mutable_store("test_store");
    std::string key = "overwrite_key";
    mutable_value value1 = make_blob("first value");
    mutable_value value2 = make_blob("second value");

    // First put stores the original value
    cppcoro::sync_wait(store->put(key, value1));

    // Verify the first value is stored
    auto retrieved1 = cppcoro::sync_wait(store->get(key));
    REQUIRE(retrieved1.has_value());
    REQUIRE(to_string(*retrieved1) == "first value");

    // Second put with the same key but different content should replace
    // (FR-16) - KEY DIFFERENTIATOR from CAS/AC idempotent behavior
    cppcoro::sync_wait(store->put(key, value2));

    // Verify the most recent value is returned (overwrite occurred)
    auto retrieved2 = cppcoro::sync_wait(store->get(key));
    REQUIRE(retrieved2.has_value());
    REQUIRE(to_string(*retrieved2) == "second value");
}

TEST_CASE("memory_mutable_store - miss returns nullopt", tag)
{
    auto store = make_memory_mutable_store("test_store");
    std::string key = "never_stored_key";

    // Get on a never-stored key should return nullopt (FR-17)
    auto retrieved = cppcoro::sync_wait(store->get(key));
    REQUIRE_FALSE(retrieved.has_value());
}

TEST_CASE("memory_mutable_store - exists", tag)
{
    auto store = make_memory_mutable_store("test_store");
    std::string key = "exists_test_key";
    mutable_value value = make_blob("some content");

    // exists should be false before put (FR-18)
    bool exists_before = cppcoro::sync_wait(store->exists(key));
    REQUIRE_FALSE(exists_before);

    // Put the value
    cppcoro::sync_wait(store->put(key, value));

    // exists should be true after put (FR-18)
    bool exists_after = cppcoro::sync_wait(store->exists(key));
    REQUIRE(exists_after);
}

TEST_CASE("memory_mutable_store - opaque value only / no schema", tag)
{
    auto store = make_memory_mutable_store("test_store");

    // Demonstrate arbitrary opaque byte values stored/retrieved verbatim
    // with no interpretation (FR-19)

    // A value that looks like a jobs/<id> string payload
    std::string key1 = "job_123";
    mutable_value value1 = make_blob("jobs/abc-def-789");
    cppcoro::sync_wait(store->put(key1, value1));

    // An unrelated binary-looking blob (different arbitrary content)
    std::string key2 = "binary_data";
    byte_vector binary_data{0x01, 0x02, 0x03, 0xFF, 0xAB, 0xCD};
    mutable_value value2 = make_blob(binary_data);
    cppcoro::sync_wait(store->put(key2, value2));

    // Retrieve both and verify they are stored/retrieved verbatim
    auto retrieved1 = cppcoro::sync_wait(store->get(key1));
    REQUIRE(retrieved1.has_value());
    REQUIRE(to_string(*retrieved1) == "jobs/abc-def-789");

    auto retrieved2 = cppcoro::sync_wait(store->get(key2));
    REQUIRE(retrieved2.has_value());
    REQUIRE(retrieved2->size() == 6);
    auto const* data = retrieved2->data();
    REQUIRE(data[0] == std::byte{0x01});
    REQUIRE(data[5] == std::byte{0xCD});

    // Verify no schema interpretation: the store treats them as opaque
    // (both stored independently; retrieval is verbatim)
}

TEST_CASE(
    "memory_mutable_store - empty value is a hit distinct from miss", tag)
{
    auto store = make_memory_mutable_store("test_store");
    std::string key_empty = "empty_key";
    std::string key_missing = "missing_key";
    mutable_value empty_value = make_blob("");

    // Store an empty blob (§8)
    cppcoro::sync_wait(store->put(key_empty, empty_value));

    // Get the empty blob - should return a populated optional (a hit)
    auto retrieved_empty = cppcoro::sync_wait(store->get(key_empty));
    REQUIRE(retrieved_empty.has_value());
    REQUIRE(retrieved_empty->size() == 0);

    // exists should return true for the empty blob
    bool exists_empty = cppcoro::sync_wait(store->exists(key_empty));
    REQUIRE(exists_empty);

    // Get on a missing key - should return nullopt (a miss, distinct from
    // the empty-value hit)
    auto retrieved_missing = cppcoro::sync_wait(store->get(key_missing));
    REQUIRE_FALSE(retrieved_missing.has_value());

    // exists should return false for the missing key
    bool exists_missing = cppcoro::sync_wait(store->exists(key_missing));
    REQUIRE_FALSE(exists_missing);
}

TEST_CASE("memory_mutable_store - concurrent access", tag)
{
    auto store = make_memory_mutable_store("test_store");
    std::string key1 = "concurrent_key_1";
    std::string key2 = "concurrent_key_2";
    std::string key3 = "concurrent_key_3";
    mutable_value value1 = make_blob("concurrent value 1");
    mutable_value value2 = make_blob("concurrent value 2");
    mutable_value value3 = make_blob("concurrent value 3");

    // Launch multiple threads doing put of different keys concurrently
    // (NFR-1)
    std::vector<std::thread> threads;
    threads.emplace_back([&]() {
        cppcoro::sync_wait(store->put(key1, value1));
    });
    threads.emplace_back([&]() {
        cppcoro::sync_wait(store->put(key2, value2));
    });
    threads.emplace_back([&]() {
        cppcoro::sync_wait(store->put(key3, value3));
    });

    // Wait for all threads to complete
    for (auto& t : threads)
    {
        t.join();
    }

    // After concurrent puts, all keys should exist and have correct values
    // (no corruption or undefined behavior)
    bool exists1 = cppcoro::sync_wait(store->exists(key1));
    bool exists2 = cppcoro::sync_wait(store->exists(key2));
    bool exists3 = cppcoro::sync_wait(store->exists(key3));
    REQUIRE(exists1);
    REQUIRE(exists2);
    REQUIRE(exists3);

    auto retrieved1 = cppcoro::sync_wait(store->get(key1));
    auto retrieved2 = cppcoro::sync_wait(store->get(key2));
    auto retrieved3 = cppcoro::sync_wait(store->get(key3));
    REQUIRE(retrieved1.has_value());
    REQUIRE(to_string(*retrieved1) == "concurrent value 1");
    REQUIRE(retrieved2.has_value());
    REQUIRE(to_string(*retrieved2) == "concurrent value 2");
    REQUIRE(retrieved3.has_value());
    REQUIRE(to_string(*retrieved3) == "concurrent value 3");
}

TEST_CASE("memory_mutable_store - concurrent overwrite", tag)
{
    auto store = make_memory_mutable_store("test_store");
    std::string key = "concurrent_overwrite_key";
    mutable_value value1 = make_blob("from thread 1");
    mutable_value value2 = make_blob("from thread 2");
    mutable_value value3 = make_blob("from thread 3");

    // Launch multiple threads doing put of the same key with different
    // values (NFR-1)
    std::vector<std::thread> threads;
    threads.emplace_back([&]() {
        cppcoro::sync_wait(store->put(key, value1));
    });
    threads.emplace_back([&]() {
        cppcoro::sync_wait(store->put(key, value2));
    });
    threads.emplace_back([&]() {
        cppcoro::sync_wait(store->put(key, value3));
    });

    // Wait for all threads to complete
    for (auto& t : threads)
    {
        t.join();
    }

    // After concurrent overwrites, the key should exist (NFR-1)
    bool exists = cppcoro::sync_wait(store->exists(key));
    REQUIRE(exists);

    // Get should return one of the stored values (whichever won the race)
    auto retrieved = cppcoro::sync_wait(store->get(key));
    REQUIRE(retrieved.has_value());

    // The retrieved value should be one of the three (proving no corruption)
    std::string retrieved_str = to_string(*retrieved);
    bool is_valid = (retrieved_str == "from thread 1"
                     || retrieved_str == "from thread 2"
                     || retrieved_str == "from thread 3");
    REQUIRE(is_valid);
}

TEST_CASE("memory_mutable_store - name accessor", tag)
{
    auto store = make_memory_mutable_store("my_custom_store_name");

    // Verify the name accessor returns the factory-supplied name
    REQUIRE(store->name() == "my_custom_store_name");

    // Also test the default constructor name
    auto default_store = make_memory_mutable_store("another_name");
    REQUIRE(default_store->name() == "another_name");
}
