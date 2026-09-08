#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/content_helpers.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/plugins/secondary_cache/local/disk_cas.h>
#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

using namespace cradle;

namespace {

char const tag[] = "[disk_cas]";

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
    bool check_file_data,
    std::optional<std::size_t> size_limit,
    bool start_empty)
{
    durable_store_settings settings;
    settings.directory = directory;
    settings.size_limit = size_limit;
    settings.check_file_data = check_file_data;
    settings.start_empty = start_empty;
    return settings;
}

// A blob small enough to be stored inline in the index.
blob
small_content()
{
    return make_blob("small durable content");
}

// A blob large enough to be stored in an external, compressed file. Its bytes
// vary so the round-trip check is meaningful.
blob
large_content()
{
    std::string data;
    data.reserve(4096);
    for (int i = 0; i != 4096; ++i)
    {
        data.push_back(static_cast<char>('A' + (i % 53)));
    }
    return make_blob(std::move(data));
}

} // namespace

TEST_CASE("putting an already-present digest again is a no-op", tag)
{
    temp_directory const dir{"disk_cas_idempotent"};
    auto store = make_local_durable_store(
        make_settings(dir.path, true, std::nullopt, true));
    auto cas = make_disk_cas(store, "cas");

    auto content = small_content();
    auto d = digest_of(content);

    cppcoro::sync_wait(cas->put(d, content));
    // A second put of the same digest+content must not error and must leave
    // the stored content unchanged.
    REQUIRE_NOTHROW(cppcoro::sync_wait(cas->put(d, content)));

    auto result = cppcoro::sync_wait(cas->get(d));
    REQUIRE(result);
    REQUIRE(*result == content);
}

TEST_CASE("get round-trips inline content byte-for-byte", tag)
{
    temp_directory const dir{"disk_cas_roundtrip_inline"};
    auto store = make_local_durable_store(
        make_settings(dir.path, true, std::nullopt, true));
    auto cas = make_disk_cas(store, "cas");

    auto content = small_content();
    auto d = digest_of(content);

    cppcoro::sync_wait(cas->put(d, content));
    auto result = cppcoro::sync_wait(cas->get(d));

    REQUIRE(result);
    REQUIRE(*result == content);
}

TEST_CASE("get round-trips external content byte-for-byte", tag)
{
    temp_directory const dir{"disk_cas_roundtrip_external"};
    auto store = make_local_durable_store(
        make_settings(dir.path, true, std::nullopt, true));
    auto cas = make_disk_cas(store, "cas");

    auto content = large_content();
    auto d = digest_of(content);

    cppcoro::sync_wait(cas->put(d, content));
    auto result = cppcoro::sync_wait(cas->get(d));

    REQUIRE(result);
    REQUIRE(*result == content);
}

TEST_CASE("get of a never-stored digest returns nullopt", tag)
{
    temp_directory const dir{"disk_cas_miss"};
    auto store = make_local_durable_store(
        make_settings(dir.path, true, std::nullopt, true));
    auto cas = make_disk_cas(store, "cas");

    auto d = digest_of(make_blob("never stored"));
    auto result = cppcoro::sync_wait(cas->get(d));

    REQUIRE_FALSE(result);
}

TEST_CASE("exists is false before put and true after", tag)
{
    temp_directory const dir{"disk_cas_exists"};
    auto store = make_local_durable_store(
        make_settings(dir.path, true, std::nullopt, true));
    auto cas = make_disk_cas(store, "cas");

    auto content = small_content();
    auto d = digest_of(content);

    REQUIRE_FALSE(cppcoro::sync_wait(cas->exists(d)));
    cppcoro::sync_wait(cas->put(d, content));
    REQUIRE(cppcoro::sync_wait(cas->exists(d)));
}

TEST_CASE("caller-supplied digest is stored without re-hashing", tag)
{
    // Integrity disabled so the intentionally-mismatched digest is not
    // rejected; content kept inline to avoid the external integrity path.
    temp_directory const dir{"disk_cas_no_rehash"};
    auto store = make_local_durable_store(
        make_settings(dir.path, false, std::nullopt, true));
    auto cas = make_disk_cas(store, "cas");

    auto content = small_content();
    // An arbitrary key that is not the content's real hash.
    digest arbitrary_key{"arbitrary-not-a-real-hash-key"};
    REQUIRE(arbitrary_key != digest_of(content));

    cppcoro::sync_wait(cas->put(arbitrary_key, content));
    auto result = cppcoro::sync_wait(cas->get(arbitrary_key));

    REQUIRE(result);
    REQUIRE(*result == content);
}

TEST_CASE("stored content persists across sessions", tag)
{
    temp_directory const dir{"disk_cas_cross_session"};
    auto content = large_content();
    auto d = digest_of(content);

    // First session: reuse the directory (do not start empty) and write.
    {
        auto store = make_local_durable_store(
            make_settings(dir.path, true, std::nullopt, false));
        auto cas = make_disk_cas(store, "cas");
        cppcoro::sync_wait(cas->put(d, content));
        REQUIRE(cppcoro::sync_wait(cas->exists(d)));
    }

    // Second session: a brand-new store/adapter over the same directory must
    // find the previously-written content byte-for-byte.
    {
        auto store = make_local_durable_store(
            make_settings(dir.path, true, std::nullopt, false));
        auto cas = make_disk_cas(store, "cas");
        auto result = cppcoro::sync_wait(cas->get(d));
        REQUIRE(result);
        REQUIRE(*result == content);
    }
}

TEST_CASE("corrupted external content is never returned as valid", tag)
{
    temp_directory const dir{"disk_cas_integrity"};
    auto store = make_local_durable_store(
        make_settings(dir.path, true, std::nullopt, true));
    auto cas = make_disk_cas(store, "cas");

    auto content = large_content();
    auto d = digest_of(content);
    cppcoro::sync_wait(cas->put(d, content));

    // Truncate the external file on disk to simulate a corrupt/interrupted
    // write. The file is addressed by the content's digest.
    auto path = store->cache().get_path_for_digest(d);
    {
        std::ofstream output{
            path.string(), std::ios::out | std::ios::trunc | std::ios::binary};
        output << "garbage";
    }

    bool threw = false;
    std::optional<blob> result;
    try
    {
        result = cppcoro::sync_wait(cas->get(d));
    }
    catch (std::exception const&)
    {
        threw = true;
    }

    // It must either throw or report absent; it must never return the
    // original bytes as if valid.
    REQUIRE((threw || !result || *result != content));
}

TEST_CASE("a reclaimed digest reads back as nullopt", tag)
{
    // A small capacity forces eviction of the oldest entries once enough have
    // been stored.
    temp_directory const dir{"disk_cas_reclamation"};
    auto store = make_local_durable_store(
        make_settings(dir.path, true, std::size_t{300}, true));
    auto cas = make_disk_cas(store, "cas");

    // The first digest stored is the least-recently-used, so it is evicted
    // first once the total exceeds the capacity.
    auto first_content = make_blob("reclaim_value_first_entry_padding_bytes");
    auto first_digest = digest_of(first_content);
    cppcoro::sync_wait(cas->put(first_digest, first_content));

    for (int i = 0; i != 40; ++i)
    {
        auto c = make_blob(
            "reclaim_value_" + std::to_string(i) + "_padding_bytes_xxxx");
        cppcoro::sync_wait(cas->put(digest_of(c), c));
    }

    // The evicted entry reads back as absent, not as an error.
    std::optional<blob> result;
    REQUIRE_NOTHROW(result = cppcoro::sync_wait(cas->get(first_digest)));
    REQUIRE_FALSE(result);
}

TEST_CASE("concurrent same-digest puts leave the store consistent", tag)
{
    temp_directory const dir{"disk_cas_concurrent"};
    auto store = make_local_durable_store(
        make_settings(dir.path, true, std::nullopt, true));
    auto cas = make_disk_cas(store, "cas");

    auto content = small_content();
    auto d = digest_of(content);

    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int i = 0; i != 8; ++i)
    {
        threads.emplace_back([&]() {
            try
            {
                cppcoro::sync_wait(cas->put(d, content));
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
    REQUIRE(cppcoro::sync_wait(cas->exists(d)));
    auto result = cppcoro::sync_wait(cas->get(d));
    REQUIRE(result);
    REQUIRE(*result == content);
}
