#include <atomic>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/plugins/secondary_cache/local/disk_ac.h>
#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

using namespace cradle;

namespace {

char const tag[] = "[disk_ac]";

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

// Builds settings pointed at a temporary directory; callers tune the fields
// they care about (integrity, capacity, empty-on-open).
durable_store_settings
make_settings(
    std::string const& directory,
    std::optional<std::size_t> size_limit,
    bool start_empty)
{
    durable_store_settings settings;
    settings.directory = directory;
    settings.size_limit = size_limit;
    settings.check_file_data = true;
    settings.start_empty = start_empty;
    return settings;
}

} // namespace

TEST_CASE("get returns the digest previously associated with a key", tag)
{
    temp_directory const dir{"disk_ac_put_get"};
    auto store = make_local_durable_store(
        make_settings(dir.path, std::nullopt, true));
    auto ac = make_disk_ac(store, "ac");

    request_key key{"request-key-alpha"};
    digest value{"result-digest-alpha"};

    cppcoro::sync_wait(ac->put(key, value));
    auto result = cppcoro::sync_wait(ac->get(key));

    REQUIRE(result);
    REQUIRE(*result == value);
}

TEST_CASE(
    "a second put under an existing key leaves the original unchanged", tag)
{
    temp_directory const dir{"disk_ac_insert_if_absent"};
    auto store = make_local_durable_store(
        make_settings(dir.path, std::nullopt, true));
    auto ac = make_disk_ac(store, "ac");

    request_key key{"request-key-stable"};
    digest first{"first-digest"};
    digest second{"second-different-digest"};
    REQUIRE(first != second);

    cppcoro::sync_wait(ac->put(key, first));
    // Insert-if-absent: a later put under the same key must not overwrite the
    // original association.
    cppcoro::sync_wait(ac->put(key, second));

    auto result = cppcoro::sync_wait(ac->get(key));
    REQUIRE(result);
    REQUIRE(*result == first);
}

TEST_CASE("get of a never-associated key returns nullopt", tag)
{
    temp_directory const dir{"disk_ac_miss"};
    auto store = make_local_durable_store(
        make_settings(dir.path, std::nullopt, true));
    auto ac = make_disk_ac(store, "ac");

    auto result = cppcoro::sync_wait(ac->get(request_key{"absent-key"}));

    REQUIRE_FALSE(result);
}

TEST_CASE("exists is false before an association and true after", tag)
{
    temp_directory const dir{"disk_ac_exists"};
    auto store = make_local_durable_store(
        make_settings(dir.path, std::nullopt, true));
    auto ac = make_disk_ac(store, "ac");

    request_key key{"request-key-exists"};
    digest value{"digest-exists"};

    REQUIRE_FALSE(cppcoro::sync_wait(ac->exists(key)));
    cppcoro::sync_wait(ac->put(key, value));
    REQUIRE(cppcoro::sync_wait(ac->exists(key)));
}

TEST_CASE("distinct keys map to their own digests independently", tag)
{
    temp_directory const dir{"disk_ac_multiple"};
    auto store = make_local_durable_store(
        make_settings(dir.path, std::nullopt, true));
    auto ac = make_disk_ac(store, "ac");

    std::vector<std::pair<request_key, digest>> associations;
    for (int i = 0; i != 16; ++i)
    {
        associations.emplace_back(
            request_key{"key-" + std::to_string(i)},
            digest{"digest-" + std::to_string(i)});
    }

    for (auto const& [key, value] : associations)
    {
        cppcoro::sync_wait(ac->put(key, value));
    }

    // Each key must read back exactly its own value, none crossed.
    for (auto const& [key, value] : associations)
    {
        auto result = cppcoro::sync_wait(ac->get(key));
        REQUIRE(result);
        REQUIRE(*result == value);
    }
}

TEST_CASE("an association persists across sessions", tag)
{
    temp_directory const dir{"disk_ac_cross_session"};
    request_key key{"request-key-durable"};
    digest value{"result-digest-durable"};

    // First session: reuse the directory (do not start empty) and write.
    {
        auto store = make_local_durable_store(
            make_settings(dir.path, std::nullopt, false));
        auto ac = make_disk_ac(store, "ac");
        cppcoro::sync_wait(ac->put(key, value));
        REQUIRE(cppcoro::sync_wait(ac->exists(key)));
    }

    // Second session: a brand-new store/adapter over the same directory must
    // find the previously-written association.
    {
        auto store = make_local_durable_store(
            make_settings(dir.path, std::nullopt, false));
        auto ac = make_disk_ac(store, "ac");
        auto result = cppcoro::sync_wait(ac->get(key));
        REQUIRE(result);
        REQUIRE(*result == value);
    }
}

TEST_CASE("concurrent puts of distinct keys leave the store consistent", tag)
{
    temp_directory const dir{"disk_ac_concurrent"};
    auto store = make_local_durable_store(
        make_settings(dir.path, std::nullopt, true));
    auto ac = make_disk_ac(store, "ac");

    constexpr int thread_count = 8;
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int i = 0; i != thread_count; ++i)
    {
        threads.emplace_back([&ac, &failures, i]() {
            try
            {
                request_key key{"concurrent-key-" + std::to_string(i)};
                digest value{"concurrent-digest-" + std::to_string(i)};
                cppcoro::sync_wait(ac->put(key, value));
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

    // Every association written concurrently must be present and correct.
    for (int i = 0; i != thread_count; ++i)
    {
        request_key key{"concurrent-key-" + std::to_string(i)};
        digest value{"concurrent-digest-" + std::to_string(i)};
        auto result = cppcoro::sync_wait(ac->get(key));
        REQUIRE(result);
        REQUIRE(*result == value);
    }
}
