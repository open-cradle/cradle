#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../support/inner_service.h"
#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/memory_ac.h>
#include <cradle/inner/storage/memory_cas.h>
#include <cradle/inner/storage/memory_mutable_store.h>
#include <cradle/inner/storage/storage_config_keys.h>
#include <cradle/inner/storage/storage_registration.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][storage][registration]";

} // namespace

TEST_CASE("CAS shared instance by reference", tag)
{
    // Set a CAS store once, get it twice, and confirm both lookups return the
    // SAME instance by reference (compare addresses).
    inner_resources resources{make_inner_tests_config()};
    resources.set_cas_store(make_memory_cas("test_cas"), true);

    cas_intf& ref1 = resources.cas_store();
    cas_intf& ref2 = resources.cas_store();

    // Same instance by reference — both addresses must be identical
    REQUIRE(&ref1 == &ref2);

    // Confirm it is usable (sanity check: store and retrieve)
    auto test_blob = make_blob(std::string("hello"));
    auto digest_key = std::string("test_digest_key");

    cppcoro::sync_wait(ref1.put(digest_key, test_blob));
    auto retrieved = cppcoro::sync_wait(ref1.get(digest_key));

    REQUIRE(retrieved.has_value());
}

TEST_CASE("AC shared instance by reference", tag)
{
    inner_resources resources{make_inner_tests_config()};
    resources.set_ac_store(make_memory_ac("test_ac"), true);

    ac_intf& ref1 = resources.ac_store();
    ac_intf& ref2 = resources.ac_store();

    REQUIRE(&ref1 == &ref2);

    // Sanity check: put and get an association
    auto req_key = std::string("request_key");
    auto digest_val = std::string("digest_value");

    cppcoro::sync_wait(ref1.put(req_key, digest_val));
    auto retrieved = cppcoro::sync_wait(ref1.get(req_key));

    REQUIRE(retrieved.has_value());
    REQUIRE(*retrieved == digest_val);
}

TEST_CASE("Mutable store shared instance by reference", tag)
{
    inner_resources resources{make_inner_tests_config()};
    resources.set_mutable_store(
        make_memory_mutable_store("test_mutable"), true);

    mutable_store_intf& ref1 = resources.mutable_store();
    mutable_store_intf& ref2 = resources.mutable_store();

    REQUIRE(&ref1 == &ref2);

    // Sanity check: put and get a mutable value
    auto key = std::string("key");
    auto value = make_blob(std::string("value"));

    cppcoro::sync_wait(ref1.put(key, value));
    auto retrieved = cppcoro::sync_wait(ref1.get(key));

    REQUIRE(retrieved.has_value());
}

TEST_CASE("CAS by-name selection returns configured instance", tag)
{
    inner_resources resources{make_inner_tests_config()};

    // Register a CAS with a specific name
    std::string const cas_name = "my_cas";
    resources.set_cas_store(make_memory_cas(cas_name), true);

    // By-name lookup returns the instance registered under that name
    cas_intf& by_name = resources.cas_store(cas_name);
    cas_intf& by_default = resources.cas_store();

    // Since we registered it as default, both should be the same instance
    REQUIRE(&by_name == &by_default);
    REQUIRE(by_name.name() == cas_name);

    // Prove it's a real, selected instance: round-trip put/get
    auto digest_key = std::string("test_key");
    auto test_blob = make_blob(std::string("test_content"));

    cppcoro::sync_wait(by_name.put(digest_key, test_blob));
    auto retrieved = cppcoro::sync_wait(by_name.get(digest_key));

    REQUIRE(retrieved.has_value());
}

TEST_CASE("AC by-name selection returns configured instance", tag)
{
    inner_resources resources{make_inner_tests_config()};

    std::string const ac_name = "my_ac";
    resources.set_ac_store(make_memory_ac(ac_name), true);

    ac_intf& by_name = resources.ac_store(ac_name);
    ac_intf& by_default = resources.ac_store();

    REQUIRE(&by_name == &by_default);
    REQUIRE(by_name.name() == ac_name);

    // Round-trip put/get
    auto req_key = std::string("request_123");
    auto digest_val = std::string("digest_abc");

    cppcoro::sync_wait(by_name.put(req_key, digest_val));
    auto retrieved = cppcoro::sync_wait(by_name.get(req_key));

    REQUIRE(retrieved.has_value());
    REQUIRE(*retrieved == digest_val);
}

TEST_CASE("Mutable store by-name selection", tag)
{
    inner_resources resources{make_inner_tests_config()};

    std::string const store_name = "my_mutable";
    resources.set_mutable_store(make_memory_mutable_store(store_name), true);

    mutable_store_intf& by_name = resources.mutable_store(store_name);
    mutable_store_intf& by_default = resources.mutable_store();

    REQUIRE(&by_name == &by_default);
    REQUIRE(by_name.name() == store_name);

    // Round-trip
    auto key = std::string("some_key");
    auto value = make_blob(std::string("some_value"));

    cppcoro::sync_wait(by_name.put(key, value));
    auto retrieved = cppcoro::sync_wait(by_name.get(key));

    REQUIRE(retrieved.has_value());
}

TEST_CASE("register_storage_from_config - CAS", tag)
{
    // Build a service_config with CAS_FACTORY = MEMORY_CAS, register it, and
    // confirm the by-name getter returns a usable instance.
    service_config_map config_map{
        {storage_config_keys::CAS_FACTORY, storage_config_values::MEMORY_CAS}};
    service_config config{config_map};

    inner_resources resources{config};
    register_storage_from_config(resources);

    // The factory value is the name under which it's registered
    cas_intf& store = resources.cas_store(storage_config_values::MEMORY_CAS);

    REQUIRE(store.name() == storage_config_values::MEMORY_CAS);

    // Prove it's usable: round-trip put/get
    auto digest_key = std::string("cfg_test_key");
    auto test_blob = make_blob(std::string("cfg_test_content"));

    cppcoro::sync_wait(store.put(digest_key, test_blob));
    auto retrieved = cppcoro::sync_wait(store.get(digest_key));

    REQUIRE(retrieved.has_value());
}

TEST_CASE("register_storage_from_config - AC", tag)
{
    service_config_map config_map{
        {storage_config_keys::AC_FACTORY, storage_config_values::MEMORY_AC}};
    service_config config{config_map};

    inner_resources resources{config};
    register_storage_from_config(resources);

    ac_intf& store = resources.ac_store(storage_config_values::MEMORY_AC);

    REQUIRE(store.name() == storage_config_values::MEMORY_AC);

    // Round-trip
    auto req_key = std::string("cfg_req");
    auto digest_val = std::string("cfg_digest");

    cppcoro::sync_wait(store.put(req_key, digest_val));
    auto retrieved = cppcoro::sync_wait(store.get(req_key));

    REQUIRE(retrieved.has_value());
    REQUIRE(*retrieved == digest_val);
}

TEST_CASE("register_storage_from_config - Mutable", tag)
{
    service_config_map config_map{
        {storage_config_keys::MUTABLE_STORE_FACTORY,
         storage_config_values::MEMORY_MUTABLE_STORE}};
    service_config config{config_map};

    inner_resources resources{config};
    register_storage_from_config(resources);

    mutable_store_intf& store
        = resources.mutable_store(storage_config_values::MEMORY_MUTABLE_STORE);

    REQUIRE(store.name() == storage_config_values::MEMORY_MUTABLE_STORE);

    // Round-trip
    auto key = std::string("cfg_key");
    auto value = make_blob(std::string("cfg_value"));

    cppcoro::sync_wait(store.put(key, value));
    auto retrieved = cppcoro::sync_wait(store.get(key));

    REQUIRE(retrieved.has_value());
}

TEST_CASE("CAS unset-name lookup throws", tag)
{
    inner_resources resources{make_inner_tests_config()};

    // Looking up a store by a name that was never registered THROWS with
    // std::logic_error (matching the requests_storage(name) pattern).
    REQUIRE_THROWS_AS(
        resources.cas_store("nonexistent_cas"), std::logic_error);
}

TEST_CASE("AC unset-name lookup throws", tag)
{
    inner_resources resources{make_inner_tests_config()};

    REQUIRE_THROWS_AS(resources.ac_store("nonexistent_ac"), std::logic_error);
}

TEST_CASE("Mutable store unset-name lookup throws", tag)
{
    inner_resources resources{make_inner_tests_config()};

    REQUIRE_THROWS_AS(
        resources.mutable_store("nonexistent_mutable"), std::logic_error);
}

TEST_CASE("Default CAS not set throws", tag)
{
    // Create resources without registering a default CAS
    inner_resources resources{make_inner_tests_config()};

    // Attempt to get the default CAS should throw
    REQUIRE_THROWS_AS(resources.cas_store(), std::logic_error);
}

TEST_CASE("Default AC not set throws", tag)
{
    inner_resources resources{make_inner_tests_config()};

    REQUIRE_THROWS_AS(resources.ac_store(), std::logic_error);
}

TEST_CASE("Default mutable store not set throws", tag)
{
    inner_resources resources{make_inner_tests_config()};

    REQUIRE_THROWS_AS(resources.mutable_store(), std::logic_error);
}
