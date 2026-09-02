#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/content_helpers.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/mutable_store_intf.h>
#include <cradle/inner/storage/storage_config_keys.h>
#include <cradle/plugins/secondary_cache/local/durable_storage_registration.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

using namespace cradle;

namespace {

char const tag[] = "[durable_storage_registration]";

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

// Selects all three durable (on-disk) backends by name; the caller layers
// directory / size-limit keys on top for the location and capacity it needs.
service_config_map
all_disk_factories()
{
    return service_config_map{
        {storage_config_keys::CAS_FACTORY,
         durable_storage_config_values::DISK_CAS},
        {storage_config_keys::AC_FACTORY,
         durable_storage_config_values::DISK_AC},
        {storage_config_keys::MUTABLE_STORE_FACTORY,
         durable_storage_config_values::DISK_MUTABLE_STORE}};
}

} // namespace

TEST_CASE(
    "durable registration - each capability selectable as its disk backend",
    tag)
{
    // With each factory key set to its disk_* value and a distinct directory
    // per capability, every registered instance is usable and satisfies its
    // M1 contract.
    temp_directory const cas_dir{"durable_reg_sel_cas"};
    temp_directory const ac_dir{"durable_reg_sel_ac"};
    temp_directory const mut_dir{"durable_reg_sel_mutable"};

    auto config_map = all_disk_factories();
    config_map[durable_storage_config_keys::CAS_DIRECTORY] = cas_dir.path;
    config_map[durable_storage_config_keys::AC_DIRECTORY] = ac_dir.path;
    config_map[durable_storage_config_keys::MUTABLE_STORE_DIRECTORY]
        = mut_dir.path;

    inner_resources resources{service_config{config_map}};
    register_local_durable_storage_from_config(resources);

    // CAS: put/get by digest.
    auto content = make_blob("selection cas content");
    auto d = digest_of(content);
    cppcoro::sync_wait(resources.cas_store().put(d, content));
    auto cas_result = cppcoro::sync_wait(resources.cas_store().get(d));
    REQUIRE(cas_result);
    REQUIRE(*cas_result == content);

    // AC: put/get association.
    request_key rk{"selection request key"};
    digest assoc{"selection digest value"};
    cppcoro::sync_wait(resources.ac_store().put(rk, assoc));
    auto ac_result = cppcoro::sync_wait(resources.ac_store().get(rk));
    REQUIRE(ac_result);
    REQUIRE(*ac_result == assoc);

    // Mutable store: put/get with most-recent-write-wins overwrite.
    std::string mk{"selection mutable key"};
    auto v1 = make_blob("selection first value");
    auto v2 = make_blob("selection second value");
    REQUIRE_FALSE(v1 == v2);
    cppcoro::sync_wait(resources.mutable_store().put(mk, v1));
    cppcoro::sync_wait(resources.mutable_store().put(mk, v2));
    auto mut_result = cppcoro::sync_wait(resources.mutable_store().get(mk));
    REQUIRE(mut_result);
    REQUIRE(*mut_result == v2);
}

TEST_CASE(
    "durable registration - shared default directory serves all capabilities",
    tag)
{
    // With only the shared disk_cache/directory set and no per-capability
    // overrides, all three capabilities are registered and round-trip against
    // that one directory.
    temp_directory const shared_dir{"durable_reg_shared_default"};

    auto config_map = all_disk_factories();
    config_map[local_disk_cache_config_keys::DIRECTORY] = shared_dir.path;

    inner_resources resources{service_config{config_map}};
    register_local_durable_storage_from_config(resources);

    auto content = make_blob("shared cas content");
    auto d = digest_of(content);
    cppcoro::sync_wait(resources.cas_store().put(d, content));

    request_key rk{"shared request key"};
    digest assoc{"shared digest value"};
    cppcoro::sync_wait(resources.ac_store().put(rk, assoc));

    std::string mk{"shared mutable key"};
    auto mv = make_blob("shared mutable value");
    cppcoro::sync_wait(resources.mutable_store().put(mk, mv));

    // All three capabilities operate correctly against the single directory.
    auto cas_result = cppcoro::sync_wait(resources.cas_store().get(d));
    REQUIRE(cas_result);
    REQUIRE(*cas_result == content);
    auto ac_result = cppcoro::sync_wait(resources.ac_store().get(rk));
    REQUIRE(ac_result);
    REQUIRE(*ac_result == assoc);
    auto mut_result = cppcoro::sync_wait(resources.mutable_store().get(mk));
    REQUIRE(mut_result);
    REQUIRE(*mut_result == mv);
}

TEST_CASE("durable registration - per-capability directory isolates CAS", tag)
{
    // A per-capability disk_cache/cas/directory override sends CAS to its own
    // store, distinct from the shared directory used by AC and mutable store.
    temp_directory const shared_dir{"durable_reg_percap_shared"};
    temp_directory const cas_dir{"durable_reg_percap_cas"};

    auto content = make_blob("per-capability cas content");
    auto d = digest_of(content);

    {
        auto config_map = all_disk_factories();
        config_map[local_disk_cache_config_keys::DIRECTORY] = shared_dir.path;
        config_map[durable_storage_config_keys::CAS_DIRECTORY] = cas_dir.path;

        inner_resources resources{service_config{config_map}};
        register_local_durable_storage_from_config(resources);

        cppcoro::sync_wait(resources.cas_store().put(d, content));
        auto here = cppcoro::sync_wait(resources.cas_store().get(d));
        REQUIRE(here);
        REQUIRE(*here == content);
    }

    // A CAS pointed at the shared directory must NOT see the content, proving
    // it landed in the dedicated CAS directory rather than the shared one.
    {
        service_config_map config_map{
            {storage_config_keys::CAS_FACTORY,
             durable_storage_config_values::DISK_CAS},
            {local_disk_cache_config_keys::DIRECTORY, shared_dir.path}};
        inner_resources resources{service_config{config_map}};
        register_local_durable_storage_from_config(resources);

        auto miss = cppcoro::sync_wait(resources.cas_store().get(d));
        REQUIRE_FALSE(miss);
    }

    // A CAS pointed at the dedicated CAS directory reads the content back.
    {
        service_config_map config_map{
            {storage_config_keys::CAS_FACTORY,
             durable_storage_config_values::DISK_CAS},
            {local_disk_cache_config_keys::DIRECTORY, cas_dir.path}};
        inner_resources resources{service_config{config_map}};
        register_local_durable_storage_from_config(resources);

        auto hit = cppcoro::sync_wait(resources.cas_store().get(d));
        REQUIRE(hit);
        REQUIRE(*hit == content);
    }
}

TEST_CASE(
    "durable registration - capabilities sharing a directory share one store",
    tag)
{
    // CAS and AC resolve to the same directory (via matching per-capability
    // overrides); the mutable store uses a different one. The shared pair must
    // operate correctly against the common directory with no error.
    temp_directory const shared_pair_dir{"durable_reg_dedup_pair"};
    temp_directory const mut_dir{"durable_reg_dedup_mutable"};

    auto config_map = all_disk_factories();
    config_map[durable_storage_config_keys::CAS_DIRECTORY]
        = shared_pair_dir.path;
    config_map[durable_storage_config_keys::AC_DIRECTORY]
        = shared_pair_dir.path;
    config_map[durable_storage_config_keys::MUTABLE_STORE_DIRECTORY]
        = mut_dir.path;

    inner_resources resources{service_config{config_map}};
    register_local_durable_storage_from_config(resources);

    auto content = make_blob("dedup cas content");
    auto d = digest_of(content);
    REQUIRE_NOTHROW(cppcoro::sync_wait(resources.cas_store().put(d, content)));

    request_key rk{"dedup request key"};
    digest assoc{"dedup digest value"};
    REQUIRE_NOTHROW(cppcoro::sync_wait(resources.ac_store().put(rk, assoc)));

    // Both capabilities backed by the one shared directory round-trip.
    auto cas_result = cppcoro::sync_wait(resources.cas_store().get(d));
    REQUIRE(cas_result);
    REQUIRE(*cas_result == content);
    auto ac_result = cppcoro::sync_wait(resources.ac_store().get(rk));
    REQUIRE(ac_result);
    REQUIRE(*ac_result == assoc);
}

TEST_CASE(
    "durable registration - registered instances persist across sessions", tag)
{
    // Write through the registered instances, tear the resources down, then
    // rebuild resources at the SAME directories and confirm the data survives
    // (including that the latest mutable overwrite persists).
    temp_directory const cas_dir{"durable_reg_xsession_cas"};
    temp_directory const ac_dir{"durable_reg_xsession_ac"};
    temp_directory const mut_dir{"durable_reg_xsession_mutable"};

    auto make_config = [&]() {
        auto config_map = all_disk_factories();
        config_map[durable_storage_config_keys::CAS_DIRECTORY] = cas_dir.path;
        config_map[durable_storage_config_keys::AC_DIRECTORY] = ac_dir.path;
        config_map[durable_storage_config_keys::MUTABLE_STORE_DIRECTORY]
            = mut_dir.path;
        return service_config{config_map};
    };

    auto content = make_blob("cross-session cas content");
    auto d = digest_of(content);
    request_key rk{"cross-session request key"};
    digest assoc{"cross-session digest value"};
    std::string mk{"cross-session mutable key"};
    auto v1 = make_blob("cross-session first value");
    auto v2 = make_blob("cross-session second value");
    REQUIRE_FALSE(v1 == v2);

    // Session 1: write, then destroy the resources.
    {
        inner_resources resources{make_config()};
        register_local_durable_storage_from_config(resources);
        cppcoro::sync_wait(resources.cas_store().put(d, content));
        cppcoro::sync_wait(resources.ac_store().put(rk, assoc));
        cppcoro::sync_wait(resources.mutable_store().put(mk, v1));
        cppcoro::sync_wait(resources.mutable_store().put(mk, v2));
    }

    // Session 2: fresh resources at the same directories read the data back.
    {
        inner_resources resources{make_config()};
        register_local_durable_storage_from_config(resources);

        auto cas_result = cppcoro::sync_wait(resources.cas_store().get(d));
        REQUIRE(cas_result);
        REQUIRE(*cas_result == content);

        auto ac_result = cppcoro::sync_wait(resources.ac_store().get(rk));
        REQUIRE(ac_result);
        REQUIRE(*ac_result == assoc);

        auto mut_result
            = cppcoro::sync_wait(resources.mutable_store().get(mk));
        REQUIRE(mut_result);
        REQUIRE(*mut_result == v2);
    }
}

TEST_CASE(
    "durable registration - distinct directories do not observe each other",
    tag)
{
    // Two registrations at distinct directories are isolated: keys written
    // through the first are misses through the second.
    temp_directory const dir_a{"durable_reg_isolation_a"};
    temp_directory const dir_b{"durable_reg_isolation_b"};

    auto config_for = [](std::string const& dir) {
        auto config_map = all_disk_factories();
        config_map[local_disk_cache_config_keys::DIRECTORY] = dir;
        return service_config{config_map};
    };

    auto content = make_blob("isolation cas content");
    auto d = digest_of(content);
    request_key rk{"isolation request key"};
    digest assoc{"isolation digest value"};
    std::string mk{"isolation mutable key"};
    auto mv = make_blob("isolation mutable value");

    inner_resources resources_a{config_for(dir_a.path)};
    register_local_durable_storage_from_config(resources_a);
    cppcoro::sync_wait(resources_a.cas_store().put(d, content));
    cppcoro::sync_wait(resources_a.ac_store().put(rk, assoc));
    cppcoro::sync_wait(resources_a.mutable_store().put(mk, mv));

    inner_resources resources_b{config_for(dir_b.path)};
    register_local_durable_storage_from_config(resources_b);

    REQUIRE_FALSE(cppcoro::sync_wait(resources_b.cas_store().get(d)));
    REQUIRE_FALSE(cppcoro::sync_wait(resources_b.ac_store().get(rk)));
    REQUIRE_FALSE(cppcoro::sync_wait(resources_b.mutable_store().get(mk)));
}

TEST_CASE("durable registration - unrecognized backend value throws", tag)
{
    // A factory key set to an unknown value makes the dispatcher throw.
    temp_directory const dir{"durable_reg_unknown_value"};

    SECTION("unknown CAS value")
    {
        service_config_map config_map{
            {storage_config_keys::CAS_FACTORY, std::string{"no_such_backend"}},
            {local_disk_cache_config_keys::DIRECTORY, dir.path}};
        inner_resources resources{service_config{config_map}};
        REQUIRE_THROWS_AS(
            register_local_durable_storage_from_config(resources),
            config_error);
    }

    SECTION("unknown AC value")
    {
        service_config_map config_map{
            {storage_config_keys::AC_FACTORY, std::string{"no_such_backend"}},
            {local_disk_cache_config_keys::DIRECTORY, dir.path}};
        inner_resources resources{service_config{config_map}};
        REQUIRE_THROWS_AS(
            register_local_durable_storage_from_config(resources),
            config_error);
    }

    SECTION("unknown mutable-store value")
    {
        service_config_map config_map{
            {storage_config_keys::MUTABLE_STORE_FACTORY,
             std::string{"no_such_backend"}},
            {local_disk_cache_config_keys::DIRECTORY, dir.path}};
        inner_resources resources{service_config{config_map}};
        REQUIRE_THROWS_AS(
            register_local_durable_storage_from_config(resources),
            config_error);
    }
}

TEST_CASE(
    "durable registration - reclamation surfaces evicted CAS as absent", tag)
{
    // Integration-level integrity/reclamation check through a registered
    // instance: a small size_limit reclaims the least-recently-used entry,
    // which then reads back as absent (not as an error) via cas_store().
    // Adapter-level reclamation is already covered in the disk_cas tests, so
    // this exercises only the registered path.
    temp_directory const cas_dir{"durable_reg_reclamation_cas"};

    service_config_map config_map{
        {storage_config_keys::CAS_FACTORY,
         durable_storage_config_values::DISK_CAS},
        {local_disk_cache_config_keys::DIRECTORY, cas_dir.path},
        {local_disk_cache_config_keys::SIZE_LIMIT, std::size_t{300}}};
    inner_resources resources{service_config{config_map}};
    register_local_durable_storage_from_config(resources);

    // The first digest stored is the least-recently-used and is evicted first
    // once the total exceeds the configured capacity.
    auto first_content = make_blob("reclaim_value_first_entry_padding_bytes");
    auto first_digest = digest_of(first_content);
    cppcoro::sync_wait(resources.cas_store().put(first_digest, first_content));

    for (int i = 0; i != 40; ++i)
    {
        auto c = make_blob(
            "reclaim_value_" + std::to_string(i) + "_padding_bytes_xxxx");
        cppcoro::sync_wait(resources.cas_store().put(digest_of(c), c));
    }

    std::optional<blob> result;
    REQUIRE_NOTHROW(
        result = cppcoro::sync_wait(resources.cas_store().get(first_digest)));
    REQUIRE_FALSE(result);
}
