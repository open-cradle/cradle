#include <chrono>
#include <filesystem>
#include <thread>

#include <catch2/catch.hpp>
#include <sqlite3.h>

#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/base64.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/utilities/text.h>
#include <cradle/plugins/secondary_cache/local/ll_disk_cache.h>

using namespace cradle;

namespace {

static char const tag[] = "[ll_disk_cache]";

ll_disk_cache_config
create_config(std::string const& cache_dir)
{
    ll_disk_cache_config config;
    config.directory = cache_dir;
    // Given the way that the value strings are generated below, this is
    // enough to hold a little under 20 items (which matters for testing
    // the eviction behavior).
    config.size_limit = 500;
    return config;
}

void
check_initial_cache(ll_disk_cache& cache, std::string const& cache_dir)
{
    auto info = cache.get_summary_info();
    REQUIRE(info.directory == cache_dir);
    REQUIRE(info.ac_entry_count == 0);
    REQUIRE(info.cas_entry_count == 0);
    REQUIRE(info.total_size == 0);
}

ll_disk_cache
create_disk_cache()
{
    std::string const cache_dir = "disk_cache";
    reset_directory(cache_dir);
    ll_disk_cache cache{create_config(cache_dir)};
    check_initial_cache(cache, cache_dir);
    return cache;
}

void
reset_disk_cache(ll_disk_cache& cache, std::string const& cache_dir)
{
    reset_directory(cache_dir);
    cache.reset(create_config(cache_dir));
    check_initial_cache(cache, cache_dir);
}

// Generate some (meaningless) key std::string for the item with the given ID.
std::string
generate_key_string(int item_id)
{
    return "meaningless_key_string_" + lexical_cast<std::string>(item_id);
}

// Generate some (meaningless) value string for the item with the given ID.
std::string
generate_value_string(int item_id)
{
    return "meaningless_value_string_" + lexical_cast<std::string>(item_id);
}

bool
test_item_access(
    ll_disk_cache& cache,
    bool external,
    std::string const& key,
    std::string const& string_value,
    bool ac_only)
{
    auto value{make_blob(string_value)};
    auto digest = get_unique_string_tmpl(value);

    auto entry = cache.find(key);
    cache.flush_ac_usage(true);
    if (external)
    {
        // Use external storage.
        auto path = cache.get_path_for_digest(digest);
        if (entry)
        {
            auto cached_contents = make_blob(read_file_contents(path));
            REQUIRE(cached_contents == value);
            return true;
        }
        else
        {
            auto opt_cas_id = cache.initiate_insert(key, digest);
            if (ac_only)
            {
                REQUIRE(!opt_cas_id);
            }
            else
            {
                REQUIRE(opt_cas_id);
                auto size = value.size();
                auto original_size = size;
                dump_string_to_file(path, string_value);
                cache.finish_insert(*opt_cas_id, size, original_size);
            }
            // Check that it's been added in the database.
            auto new_entry = cache.find(key);
            REQUIRE(new_entry);
            // Start it all again to test update behavior; this time there's no
            // need to finish.
            opt_cas_id = cache.initiate_insert(key, digest);
            REQUIRE(!opt_cas_id);
            return false;
        }
    }
    else
    {
        // Use in-database storage.
        if (entry)
        {
            REQUIRE(entry->value);
            REQUIRE(*entry->value == value);
            return true;
        }
        else
        {
            cache.insert(key, digest, value);
            // Check that it's been added.
            auto new_entry = cache.find(key);
            REQUIRE(new_entry);
            REQUIRE(new_entry->value);
            REQUIRE(*new_entry->value == value);
            // Overwrite it with a dummy value.
            // TODO overwriting with different value should never happen
            cache.insert(key, digest, make_string_literal_blob("overwritten"));
            // Do it all again to test update behavior.
            cache.insert(key, digest, value);
            new_entry = cache.find(key);
            REQUIRE(new_entry);
            REQUIRE(new_entry->value);
            REQUIRE(*new_entry->value == value);
            return false;
        }
    }
}

// Test access to an item. - This simulates access to an item via the disk
// cache. It works whether or not the item is already cached. (It will insert
// it if it's not already there.) It tests various steps along the way,
// including whether or not the cached item was valid.
//
// Since there are two methods of storing data in the cache (inside the
// database or externally in files), this uses the in-database method for
// even-numbered item IDs and the external method for odd-numbered IDs.
//
// The return value indicates whether or not the item was already cached.
//
bool
test_item_access(ll_disk_cache& cache, int item_id)
{
    bool external = item_id % 2 == 1;
    auto key = generate_key_string(item_id);
    auto value = generate_value_string(item_id);
    bool ac_only{false};

    return test_item_access(cache, external, key, value, ac_only);
}

} // namespace

TEST_CASE("simple item access", tag)
{
    auto cache{create_disk_cache()};
    // The first time, it shouldn't be in the cache, but the second time, it
    // should be.
    REQUIRE(!test_item_access(cache, 0));
    REQUIRE(test_item_access(cache, 0));
    REQUIRE(!test_item_access(cache, 1));
    REQUIRE(test_item_access(cache, 1));
}

static void
test_different_keys_with_the_same_value(bool external)
{
    auto cache{create_disk_cache()};
    std::string key0{"key0"};
    std::string key1{"key1"};
    std::string value{"shared_value"};
    bool ac_only{false};

    REQUIRE(!test_item_access(cache, external, key0, value, ac_only));
    REQUIRE(test_item_access(cache, external, key0, value, ac_only));
    auto summary0 = cache.get_summary_info();
    REQUIRE(summary0.ac_entry_count == 1);
    REQUIRE(summary0.cas_entry_count == 1);

    // Add an item with a different key but the same value; this should only
    // create a new AC entry, referring to the existing CAS entry.
    ac_only = true;
    REQUIRE(!test_item_access(cache, external, key1, value, ac_only));
    REQUIRE(test_item_access(cache, external, key1, value, ac_only));
    auto summary1 = cache.get_summary_info();
    REQUIRE(summary1.ac_entry_count == 2);
    REQUIRE(summary1.cas_entry_count == 1);
    REQUIRE(summary1.total_size == summary0.total_size);
}

TEST_CASE("different keys with the same value - internal", tag)
{
    test_different_keys_with_the_same_value(false);
}

TEST_CASE("different keys with the same value - external", tag)
{
    test_different_keys_with_the_same_value(true);
}

static void
test_look_up_non_existing_ac_entry(bool external)
{
    auto cache{create_disk_cache()};
    std::string key0{"key0 - inserted"};
    std::string key1{"key1 - not existing"};
    std::string value{"value"};
    bool ac_only{false};

    REQUIRE(!test_item_access(cache, external, key0, value, ac_only));
    auto summary0 = cache.get_summary_info();
    REQUIRE(summary0.ac_entry_count == 1);
    REQUIRE(summary0.cas_entry_count == 1);

    auto opt_ac_id = cache.look_up_ac_id(key1);
    REQUIRE(!opt_ac_id);
    auto summary1 = cache.get_summary_info();
    REQUIRE(summary1.ac_entry_count == 1);
    REQUIRE(summary1.cas_entry_count == 1);
}

TEST_CASE("look up non-existing AC entry - internal", tag)
{
    test_look_up_non_existing_ac_entry(false);
}

TEST_CASE("look up non-existing AC entry - external", tag)
{
    test_look_up_non_existing_ac_entry(true);
}

TEST_CASE("look up invalid entry - external", tag)
{
    auto cache{create_disk_cache()};
    std::string key{"key"};
    std::string value{"value"};
    auto digest{get_unique_string_tmpl(value)};

    auto opt_cas_id0 = cache.initiate_insert(key, digest);
    REQUIRE(opt_cas_id0);
    auto opt_entry = cache.find(key);
    REQUIRE(!opt_entry);

    auto opt_cas_id1 = cache.initiate_insert(key, digest);
    REQUIRE(!opt_cas_id1);
    // No cache.finish_insert(*opt_cas_id1) follow-up possible.
}

TEST_CASE("multiple initializations", tag)
{
    auto cache{create_disk_cache()};
    reset_disk_cache(cache, "alt_disk_cache");
    // Test that it can still handle basic operations.
    REQUIRE(!test_item_access(cache, 0));
    REQUIRE(test_item_access(cache, 0));
    REQUIRE(!test_item_access(cache, 1));
    REQUIRE(test_item_access(cache, 1));
}

TEST_CASE("clearing", tag)
{
    auto cache{create_disk_cache()};
    REQUIRE(!test_item_access(cache, 0));
    REQUIRE(!test_item_access(cache, 1));
    REQUIRE(test_item_access(cache, 0));
    REQUIRE(test_item_access(cache, 1));
    cache.clear();
    REQUIRE(!test_item_access(cache, 0));
    REQUIRE(!test_item_access(cache, 1));
}

TEST_CASE("LRU removal", tag)
{
    auto cache{create_disk_cache()};
    test_item_access(cache, 0);
    test_item_access(cache, 1);
    // This pattern of access should ensure that entries 0 and 1 always remain
    // in the cache while other low-numbered entries eventually get evicted.
    for (int i = 2; i != 30; ++i)
    {
        INFO(i)
        REQUIRE(test_item_access(cache, 0));
        REQUIRE(test_item_access(cache, 1));
        REQUIRE(!test_item_access(cache, i));
        // SQLite only maintains millisecond precision on its timestamps, so
        // introduce a delay here to ensure that the timestamps in the cache
        // are unique.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(test_item_access(cache, 0));
    REQUIRE(test_item_access(cache, 1));
    for (int i = 2; i != 10; ++i)
    {
        REQUIRE(!test_item_access(cache, i));
    }
}

TEST_CASE("entry removal error", tag)
{
    auto cache{create_disk_cache()};

    // Access item 1 and then open the file that holds it to create a lock on
    // it.
    test_item_access(cache, 1);
    auto key1{generate_key_string(1)};
    auto opt_entry1{cache.find(key1)};
    REQUIRE(opt_entry1);
    std::ifstream item1;
    open_file(
        item1,
        cache.get_path_for_digest(opt_entry1->digest),
        std::ios::in | std::ios::binary);

    // Now access a bunch of other items to force item 1 to be evicted.
    for (int i = 2; i != 30; ++i)
    {
        INFO(i)
        REQUIRE(!test_item_access(cache, i));
        // SQLite only maintains millisecond precision on its timestamps, so
        // introduce a delay here to ensure that the timestamps in the cache
        // are unique.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    item1.close();

    // Ensure that item 1 can still be accessed.
    test_item_access(cache, 1);
}

TEST_CASE("manual entry removal", tag)
{
    auto cache{create_disk_cache()};
    for (int i = 0; i != 2; ++i)
    {
        // Insert the item and check that it was inserted.
        REQUIRE(!test_item_access(cache, i));
        REQUIRE(test_item_access(cache, i));
        // Remove it.
        {
            auto opt_ac_id = cache.look_up_ac_id(generate_key_string(i));
            if (opt_ac_id)
            {
                cache.remove_entry(*opt_ac_id);
            }
        }
        // Check that it's not there.
        REQUIRE(!test_item_access(cache, i));
    }
}

TEST_CASE("cache summary info", tag)
{
    auto cache{create_disk_cache()};

    int64_t expected_size = 0;
    int64_t expected_ac_count = 0;
    auto check_summary_info = [&]() {
        auto summary = cache.get_summary_info();
        REQUIRE(summary.ac_entry_count == expected_ac_count);
        REQUIRE(summary.total_size == expected_size);
    };

    // Test an empty cache.
    check_summary_info();

    // Add an entry.
    test_item_access(cache, 0);
    expected_size += generate_value_string(0).length();
    ++expected_ac_count;
    check_summary_info();

    // Add another entry.
    test_item_access(cache, 1);
    expected_size += generate_value_string(1).length();
    ++expected_ac_count;
    check_summary_info();

    // Add another entry.
    test_item_access(cache, 2);
    expected_size += generate_value_string(2).length();
    ++expected_ac_count;
    check_summary_info();

    // Remove an entry.
    {
        auto opt_ac_id = cache.look_up_ac_id(generate_key_string(0));
        if (opt_ac_id)
        {
            cache.remove_entry(*opt_ac_id);
        }
    }
    expected_size -= generate_value_string(0).length();
    --expected_ac_count;
    check_summary_info();
}

TEST_CASE("corrupt cache", tag)
{
    // Set up an invalid cache directory.
    reset_directory("disk_cache");
    dump_string_to_file("disk_cache/index.db", "invalid database contents");
    file_path extraneous_file("disk_cache/some_other_file");
    dump_string_to_file(extraneous_file, "abc");

    // Check that the cache still initializes and that the extraneous file
    // is removed.
    auto cache{create_disk_cache()};
    REQUIRE(!exists(extraneous_file));
}

TEST_CASE("incompatible cache", tag)
{
    // Set up a cache directory with an incompatible database version number.
    reset_directory("disk_cache");
    {
        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open("disk_cache/index.db", &db) == SQLITE_OK);
        REQUIRE(
            sqlite3_exec(db, "pragma user_version = 9600;", 0, 0, 0)
            == SQLITE_OK);
        sqlite3_close(db);
    }
    file_path extraneous_file("disk_cache/some_other_file");
    dump_string_to_file(extraneous_file, "abc");

    // Check that the cache still initializes and that the extraneous file
    // is removed.
    auto cache{create_disk_cache()};
    REQUIRE(!exists(extraneous_file));
}

TEST_CASE("recover from a corrupt index.db", tag)
{
    reset_directory("disk_cache");
    dump_string_to_file("disk_cache/index.db", "not a database file");
    auto config{create_config("disk_cache")};
    REQUIRE_NOTHROW(std::make_unique<ll_disk_cache>(config));
}

TEST_CASE("recover from a missing finish_insert", tag)
{
    std::string cache_dir{"disk_cache"};
    reset_directory(cache_dir);
    std::string key0{"key0"};
    std::string digest0{"digest0"};
    std::size_t size0{3};
    std::string key1{"key1"};
    std::string digest1{"digest1"};
    std::size_t size1{5};

    // Simulate a first process run initiating two inserts, but finishing only
    // one: the process gets killed before it can finish the second one.
    {
        ll_disk_cache cache{create_config(cache_dir)};
        auto opt_cas_id0 = cache.initiate_insert(key0, digest0);
        REQUIRE(opt_cas_id0);
        auto opt_cas_id1 = cache.initiate_insert(key1, digest1);
        REQUIRE(opt_cas_id1);
        cache.finish_insert(*opt_cas_id0, size0, size0);
        // No finish_insert(*opt_cas_id1);
    }

    // The second process run finds a database with invalid entries. It should
    // be able to replace them with valid ones.
    {
        ll_disk_cache cache{create_config(cache_dir)};

        // Only the first entry can be found.
        auto opt_entry0 = cache.find(key0);
        REQUIRE(opt_entry0);
        REQUIRE(static_cast<std::size_t>(opt_entry0->size) == size0);
        auto opt_entry1 = cache.find(key1);
        REQUIRE(!opt_entry1);

        // The cache re-initialization should have deleted any invalid entries.
        auto info = cache.get_summary_info();
        REQUIRE(info.ac_entry_count == 1);
        REQUIRE(info.cas_entry_count == 1);

        // Properly insert the second entry; after this, the cache should
        // contain both entries.
        auto opt_cas_id1 = cache.initiate_insert(key1, digest1);
        REQUIRE(opt_cas_id1);
        cache.finish_insert(*opt_cas_id1, size1, size1);

        opt_entry1 = cache.find(key1);
        REQUIRE(opt_entry1);
        REQUIRE(static_cast<std::size_t>(opt_entry1->size) == size1);
        info = cache.get_summary_info();
        REQUIRE(info.ac_entry_count == 2);
        REQUIRE(info.cas_entry_count == 2);
    }
}
