#include <atomic>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/mutable_store_intf.h>
#include <cradle/plugins/secondary_cache/local/disk_mutable_store.h>
#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

using namespace cradle;

namespace {

char const tag[] = "[disk_mutable_store]";

// Removes a test-created directory on scope exit so each test leaves no
// artifacts behind, even if an assertion aborts the body.
struct temp_directory
{
    std::string path;

    explicit temp_directory(std::string dir) : path{std::move(dir)}
    {
        reset_directory(path);
    }

    ~temp_directory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

// Builds settings pointed at a temporary directory; callers tune whether the
// location is reused (start_empty=false) or cleared on open.
durable_store_settings
make_settings(std::string const& directory, bool start_empty)
{
    durable_store_settings settings;
    settings.directory = directory;
    settings.size_limit = std::nullopt;
    settings.check_file_data = true;
    settings.start_empty = start_empty;
    return settings;
}

// A blob whose bytes span the full 0..255 range (including embedded NULs and
// other non-text bytes) so verbatim, schema-free storage can be verified.
blob
binary_content()
{
    std::string data;
    data.reserve(256);
    for (int i = 0; i != 256; ++i)
    {
        data.push_back(static_cast<char>(i));
    }
    return make_blob(std::move(data));
}

} // namespace

TEST_CASE(
    "disk_mutable_store - get returns the value previously put under a key",
    tag)
{
    temp_directory const dir{"disk_mutable_store_put_get"};
    auto store = make_local_durable_store(make_settings(dir.path, true));
    auto mut = make_disk_mutable_store(store, "mutable_store");

    std::string key{"mutable-key-alpha"};
    auto value = make_blob("mutable value alpha");

    cppcoro::sync_wait(mut->put(key, value));
    auto result = cppcoro::sync_wait(mut->get(key));

    REQUIRE(result);
    // Read back the value byte-for-byte.
    REQUIRE(*result == value);
}

TEST_CASE(
    "disk_mutable_store - a second put under an existing key replaces the "
    "value",
    tag)
{
    temp_directory const dir{"disk_mutable_store_overwrite"};
    auto store = make_local_durable_store(make_settings(dir.path, true));
    auto mut = make_disk_mutable_store(store, "mutable_store");

    std::string key{"mutable-key-overwrite"};
    auto first = make_blob("first value");
    auto second = make_blob("second, different value");
    REQUIRE_FALSE(first == second);

    cppcoro::sync_wait(mut->put(key, first));
    // Overwrite: most recent write wins (the differentiator from the
    // insert-if-absent CAS/AC stores).
    cppcoro::sync_wait(mut->put(key, second));

    auto result = cppcoro::sync_wait(mut->get(key));
    REQUIRE(result);
    REQUIRE(*result == second);
}

TEST_CASE(
    "disk_mutable_store - get of a never-stored key returns nullopt", tag)
{
    temp_directory const dir{"disk_mutable_store_miss"};
    auto store = make_local_durable_store(make_settings(dir.path, true));
    auto mut = make_disk_mutable_store(store, "mutable_store");

    auto result = cppcoro::sync_wait(mut->get(std::string{"absent-key"}));

    REQUIRE_FALSE(result);
}

TEST_CASE(
    "disk_mutable_store - exists is false before a put and true after", tag)
{
    temp_directory const dir{"disk_mutable_store_exists"};
    auto store = make_local_durable_store(make_settings(dir.path, true));
    auto mut = make_disk_mutable_store(store, "mutable_store");

    std::string key{"mutable-key-exists"};
    auto value = make_blob("value exists");

    REQUIRE_FALSE(cppcoro::sync_wait(mut->exists(key)));
    cppcoro::sync_wait(mut->put(key, value));
    REQUIRE(cppcoro::sync_wait(mut->exists(key)));
}

TEST_CASE(
    "disk_mutable_store - arbitrary binary values round-trip verbatim", tag)
{
    temp_directory const dir{"disk_mutable_store_opaque"};
    auto store = make_local_durable_store(make_settings(dir.path, true));
    auto mut = make_disk_mutable_store(store, "mutable_store");

    std::string key{"mutable-key-binary"};
    auto value = binary_content();

    cppcoro::sync_wait(mut->put(key, value));
    auto result = cppcoro::sync_wait(mut->get(key));

    REQUIRE(result);
    // The store carries no schema; every byte comes back unchanged.
    REQUIRE(result->size() == value.size());
    REQUIRE(*result == value);
}

TEST_CASE(
    "disk_mutable_store - an empty value is a hit distinct from a miss", tag)
{
    temp_directory const dir{"disk_mutable_store_empty"};
    auto store = make_local_durable_store(make_settings(dir.path, true));
    auto mut = make_disk_mutable_store(store, "mutable_store");

    std::string stored_key{"mutable-key-empty"};
    auto empty_value = make_blob("");
    REQUIRE(empty_value.size() == 0);

    cppcoro::sync_wait(mut->put(stored_key, empty_value));

    // Storing an empty value yields a populated optional with a zero-size
    // payload, and the key is reported present.
    auto hit = cppcoro::sync_wait(mut->get(stored_key));
    REQUIRE(hit);
    REQUIRE(hit->size() == 0);
    REQUIRE(cppcoro::sync_wait(mut->exists(stored_key)));

    // A never-stored key is a distinguishable miss: empty optional and absent.
    auto miss = cppcoro::sync_wait(mut->get(std::string{"never-stored-key"}));
    REQUIRE_FALSE(miss);
    REQUIRE_FALSE(
        cppcoro::sync_wait(mut->exists(std::string{"never-stored-key"})));
}

TEST_CASE(
    "disk_mutable_store - the most recent overwrite persists across sessions",
    tag)
{
    temp_directory const dir{"disk_mutable_store_cross_session"};
    std::string key{"mutable-key-durable"};
    auto first = make_blob("durable first value");
    auto second = make_blob("durable overwritten value");
    REQUIRE_FALSE(first == second);

    // First session: reuse the directory (do not start empty), write, then
    // overwrite. Releasing the shared_ptr and adapter tears the session down.
    {
        auto store = make_local_durable_store(make_settings(dir.path, false));
        auto mut = make_disk_mutable_store(store, "mutable_store");
        cppcoro::sync_wait(mut->put(key, first));
        cppcoro::sync_wait(mut->put(key, second));
        REQUIRE(cppcoro::sync_wait(mut->exists(key)));
    }

    // Second session: a brand-new store/adapter over the same directory must
    // read back the most recently written (overwritten) value.
    {
        auto store = make_local_durable_store(make_settings(dir.path, false));
        auto mut = make_disk_mutable_store(store, "mutable_store");
        auto result = cppcoro::sync_wait(mut->get(key));
        REQUIRE(result);
        REQUIRE(*result == second);
    }
}

TEST_CASE(
    "disk_mutable_store - concurrent puts of distinct keys all succeed", tag)
{
    temp_directory const dir{"disk_mutable_store_concurrent_distinct"};
    auto store = make_local_durable_store(make_settings(dir.path, true));
    auto mut = make_disk_mutable_store(store, "mutable_store");

    constexpr int thread_count = 8;
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int i = 0; i != thread_count; ++i)
    {
        threads.emplace_back([&mut, &failures, i]() {
            try
            {
                std::string key{"concurrent-key-" + std::to_string(i)};
                auto value
                    = make_blob("concurrent-value-" + std::to_string(i));
                cppcoro::sync_wait(mut->put(key, value));
            }
            catch (...)
            {
                failures.fetch_add(1);
            }
        });
    }
    for (auto& t : threads)
    {
        t.join();
    }

    REQUIRE(failures.load() == 0);

    // Every key written concurrently reads back exactly its own value.
    for (int i = 0; i != thread_count; ++i)
    {
        std::string key{"concurrent-key-" + std::to_string(i)};
        auto value = make_blob("concurrent-value-" + std::to_string(i));
        auto result = cppcoro::sync_wait(mut->get(key));
        REQUIRE(result);
        REQUIRE(*result == value);
    }
}

TEST_CASE(
    "disk_mutable_store - concurrent overwrites of one key leave a valid "
    "value",
    tag)
{
    temp_directory const dir{"disk_mutable_store_concurrent_overwrite"};
    auto store = make_local_durable_store(make_settings(dir.path, true));
    auto mut = make_disk_mutable_store(store, "mutable_store");

    std::string key{"contended-key"};
    constexpr int thread_count = 8;
    std::set<std::string> written;
    for (int i = 0; i != thread_count; ++i)
    {
        written.insert("overwrite-value-" + std::to_string(i));
    }

    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int i = 0; i != thread_count; ++i)
    {
        threads.emplace_back([&mut, &failures, &key, i]() {
            try
            {
                auto value = make_blob("overwrite-value-" + std::to_string(i));
                cppcoro::sync_wait(mut->put(key, value));
            }
            catch (...)
            {
                failures.fetch_add(1);
            }
        });
    }
    for (auto& t : threads)
    {
        t.join();
    }

    REQUIRE(failures.load() == 0);

    // The key remains present with exactly one of the written values intact
    // (no torn or corrupted value).
    REQUIRE(cppcoro::sync_wait(mut->exists(key)));
    auto result = cppcoro::sync_wait(mut->get(key));
    REQUIRE(result);
    REQUIRE(written.count(to_string(*result)) == 1);
}
