#include <cstddef>
#include <filesystem>
#include <string>

#include <catch2/catch.hpp>

#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/service/config.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

using namespace cradle;

namespace {

char const tag[] = "[local_durable_store]";

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

} // namespace

TEST_CASE("shared defaults apply to every capability", tag)
{
    service_config_map const config_map{
        {local_disk_cache_config_keys::DIRECTORY, std::string{"shared_dir"}},
        {local_disk_cache_config_keys::SIZE_LIMIT, std::size_t{1024}},
        {local_disk_cache_config_keys::CHECK_FILE_DATA, true},
        {local_disk_cache_config_keys::START_EMPTY, true},
    };
    service_config const config{config_map};

    std::string const capabilities[] = {"cas", "ac", "mutable_store"};
    for (std::string const& capability : capabilities)
    {
        auto const settings
            = resolve_durable_store_settings(config, capability);
        REQUIRE(settings.directory == "shared_dir");
        REQUIRE(settings.size_limit == std::size_t{1024});
        REQUIRE(settings.check_file_data == true);
        REQUIRE(settings.start_empty == true);
    }
}

TEST_CASE("per-capability override wins over shared defaults", tag)
{
    service_config_map const config_map{
        {local_disk_cache_config_keys::DIRECTORY, std::string{"shared_dir"}},
        {local_disk_cache_config_keys::SIZE_LIMIT, std::size_t{1024}},
        {local_disk_cache_config_keys::CHECK_FILE_DATA, false},
        {local_disk_cache_config_keys::START_EMPTY, true},
        {durable_storage_config_keys::CAS_DIRECTORY, std::string{"cas_dir"}},
        {durable_storage_config_keys::CAS_SIZE_LIMIT, std::size_t{2048}},
        {durable_storage_config_keys::CAS_CHECK_FILE_DATA, true},
    };
    service_config const config{config_map};

    // The overridden capability uses its own directory / size_limit /
    // check_file_data, but start_empty still comes from the shared default.
    auto const cas = resolve_durable_store_settings(config, "cas");
    REQUIRE(cas.directory == "cas_dir");
    REQUIRE(cas.size_limit == std::size_t{2048});
    REQUIRE(cas.check_file_data == true);
    REQUIRE(cas.start_empty == true);

    // A capability without an override falls back to the shared defaults.
    auto const ac = resolve_durable_store_settings(config, "ac");
    REQUIRE(ac.directory == "shared_dir");
    REQUIRE(ac.size_limit == std::size_t{1024});
    REQUIRE(ac.check_file_data == false);
    REQUIRE(ac.start_empty == true);
}

TEST_CASE("construct a durable store at a temporary directory", tag)
{
    temp_directory const dir{"durable_store_test_cache"};

    durable_store_settings settings;
    settings.directory = dir.path;
    settings.size_limit = std::size_t{0x40'00'00'00U};
    settings.check_file_data = false;
    settings.start_empty = true;

    std::shared_ptr<local_durable_store> store;
    REQUIRE_NOTHROW(store = make_local_durable_store(settings));
    REQUIRE(store);

    auto const info = store->cache().get_summary_info();
    REQUIRE(info.directory == dir.path);
    REQUIRE(info.ac_entry_count == 0);
    REQUIRE(info.cas_entry_count == 0);
    REQUIRE(info.total_size == 0);
}
