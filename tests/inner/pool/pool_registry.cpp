#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <catch2/catch.hpp>
#include <cppcoro/task.hpp>

#include "../../support/inner_service.h"
#include <cradle/inner/core/exception.h>
#include <cradle/inner/pool/pool_intf.h>
#include <cradle/inner/service/resources.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][pool][registry]";

// Minimal in-test pool_intf double. It carries only a name; the store
// accessors and job operations are unused by the registry tests and throw if
// invoked, keeping the double entirely inner-lane and free of the plugins
// layer.
class test_pool : public pool_intf
{
 public:
    explicit test_pool(std::string name) : name_{std::move(name)}
    {
    }

    std::string const&
    name() const override
    {
        return name_;
    }

    cas_intf&
    cas() override
    {
        throw not_implemented_error{"test_pool::cas()"};
    }

    ac_intf&
    ac() override
    {
        throw not_implemented_error{"test_pool::ac()"};
    }

    mutable_store_intf&
    mutable_store() override
    {
        throw not_implemented_error{"test_pool::mutable_store()"};
    }

    cppcoro::task<job_id>
    submit(job_spec) override
    {
        co_return job_id{};
    }

    cppcoro::task<void>
    cancel(job_id) override
    {
        co_return;
    }

 private:
    std::string name_;
};

} // namespace

TEST_CASE("pool shared instance by reference", tag)
{
    // Set a pool once, look it up by default and by name, and confirm both
    // lookups return the SAME instance by reference (compare addresses).
    inner_resources resources{make_inner_tests_config()};

    auto owned_pool = std::make_unique<test_pool>("test_pool");
    pool_intf* raw = owned_pool.get();
    resources.set_pool(std::move(owned_pool), true);

    pool_intf& ref1 = resources.pool();
    pool_intf& ref2 = resources.pool("test_pool");

    // Same instance by reference — all three addresses must be identical.
    REQUIRE(&ref1 == &ref2);
    REQUIRE(&ref1 == raw);
    REQUIRE(ref1.name() == "test_pool");
}

TEST_CASE("pool by-name selection returns the configured instance", tag)
{
    // Register two distinct pools under different names and confirm each is
    // retrievable by its own name and is a distinct instance.
    inner_resources resources{make_inner_tests_config()};

    auto owned_a = std::make_unique<test_pool>("pool_a");
    auto owned_b = std::make_unique<test_pool>("pool_b");
    pool_intf* raw_a = owned_a.get();
    pool_intf* raw_b = owned_b.get();
    resources.set_pool(std::move(owned_a));
    resources.set_pool(std::move(owned_b));

    pool_intf& by_a = resources.pool("pool_a");
    pool_intf& by_b = resources.pool("pool_b");

    REQUIRE(&by_a == raw_a);
    REQUIRE(&by_b == raw_b);
    REQUIRE(&by_a != &by_b);
    REQUIRE(by_a.name() == "pool_a");
    REQUIRE(by_b.name() == "pool_b");
}

TEST_CASE("pool lookup throws when unset", tag)
{
    inner_resources resources{make_inner_tests_config()};

    // No pool registered: the default lookup throws.
    REQUIRE_THROWS_AS(resources.pool(), std::logic_error);

    resources.set_pool(std::make_unique<test_pool>("known_pool"));

    // A lookup by an unregistered name throws.
    REQUIRE_THROWS_AS(resources.pool("missing_pool"), std::logic_error);
}

TEST_CASE("default pool tracks the most recently set pool", tag)
{
    // set_pool assigns the default on every call, so pool() returns the
    // most-recently-set instance.
    inner_resources resources{make_inner_tests_config()};

    auto owned_a = std::make_unique<test_pool>("first");
    auto owned_b = std::make_unique<test_pool>("second");
    pool_intf* raw_b = owned_b.get();
    resources.set_pool(std::move(owned_a));
    resources.set_pool(std::move(owned_b));

    REQUIRE(&resources.pool() == raw_b);
    REQUIRE(resources.pool().name() == "second");
}
