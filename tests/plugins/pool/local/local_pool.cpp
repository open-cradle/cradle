#include <cradle/plugins/pool/local/local_pool.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../../support/inner_service.h"
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/pool/job_record.h>
#include <cradle/inner/pool/job_spec.h>
#include <cradle/inner/pool/job_types.h>
#include <cradle/inner/pool/pool_intf.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/memory_ac.h>
#include <cradle/inner/storage/memory_cas.h>
#include <cradle/inner/storage/memory_mutable_store.h>
#include <cradle/inner/storage/mutable_store_intf.h>

using namespace cradle;

namespace {

char const tag[] = "[local_pool]";

// Resolved settings with representative bounds; the exact values are not
// exercised by the record/reference/cancel behavior under test here.
pool_settings
make_test_settings()
{
    pool_settings settings;
    settings.input_size_boundary = 1024;
    settings.poll_interval = std::chrono::milliseconds{10};
    settings.poll_max_interval = std::chrono::milliseconds{100};
    return settings;
}

// A minimal single-input job spec identified by the given request key.
job_spec
make_test_job_spec(request_key key)
{
    job_spec spec;
    spec.provider = "test_provider";
    spec.key = std::move(key);
    spec.context = "";
    spec.inputs.push_back(inline_input{make_blob("input-bytes")});
    return spec;
}

// Holds the in-memory M1 stores together with a local_pool bound to them, so a
// test can both drive the pool and inspect the stores it refers to.
struct pool_fixture
{
    std::unique_ptr<inner_resources> resources{make_inner_test_resources()};
    std::unique_ptr<cas_intf> cas{make_memory_cas("pool_cas")};
    std::unique_ptr<ac_intf> ac{make_memory_ac("pool_ac")};
    std::unique_ptr<mutable_store_intf> store{
        make_memory_mutable_store("pool_store")};
    std::unique_ptr<pool_intf> pool{make_local_pool(
        "local", *resources, *cas, *ac, *store, make_test_settings())};
};

} // namespace

TEST_CASE("local_pool name reports the configured name", tag)
{
    pool_fixture fx;
    REQUIRE(fx.pool->name() == "local");
}

TEST_CASE("local_pool accessors return the same instances by reference", tag)
{
    pool_fixture fx;
    // The pool refers to the storage it was handed; it does not own or copy
    // it.
    REQUIRE(&fx.pool->cas() == fx.cas.get());
    REQUIRE(&fx.pool->ac() == fx.ac.get());
    REQUIRE(&fx.pool->mutable_store() == fx.store.get());
}

TEST_CASE("local_pool submit records a job record", tag)
{
    pool_fixture fx;
    job_spec const spec{make_test_job_spec("leaf/one")};

    job_id const id{cppcoro::sync_wait(fx.pool->submit(spec))};
    REQUIRE(!id.empty());

    // submit now dispatches a worker asynchronously, so the record may already
    // have advanced past queued by the time it is read; assert only that a
    // record exists for the assigned id under the expected key and pool.
    std::optional<mutable_value> const stored
        = cppcoro::sync_wait(fx.store->get(job_record_key(id)));
    REQUIRE(stored.has_value());

    job_record const record{deserialize_job_record(*stored)};
    REQUIRE(record.id == id);
    REQUIRE(record.key == spec.key);
    REQUIRE(record.pool_name == fx.pool->name());
}

TEST_CASE("local_pool cancel of a not-yet-terminal job records cancelled", tag)
{
    pool_fixture fx;
    job_id const id{"cancelable-job"};

    // Write a non-terminal (queued) record directly, bypassing submit so no
    // worker races the cancel. This exercises cancel's
    // read-nonterminal-then-write-cancelled logic deterministically.
    job_record queued;
    queued.id = id;
    queued.key = "leaf/cancelable";
    queued.pool_name = fx.pool->name();
    queued.status = job_status::queued;
    cppcoro::sync_wait(
        fx.store->put(job_record_key(id), serialize_job_record(queued)));

    cppcoro::sync_wait(fx.pool->cancel(id));

    std::optional<mutable_value> const stored
        = cppcoro::sync_wait(fx.store->get(job_record_key(id)));
    REQUIRE(stored.has_value());

    job_record const record{deserialize_job_record(*stored)};
    REQUIRE(record.status == job_status::cancelled);
}

TEST_CASE("local_pool cancel of an already-terminal job is a no-op", tag)
{
    pool_fixture fx;
    job_id const id{"terminal-job"};

    // Pre-store a terminal record so cancel has an existing, settled status to
    // observe.
    job_record terminal;
    terminal.id = id;
    terminal.key = "leaf/done";
    terminal.pool_name = fx.pool->name();
    terminal.status = job_status::succeeded;
    terminal.output_digest = "digest-of-result";
    cppcoro::sync_wait(
        fx.store->put(job_record_key(id), serialize_job_record(terminal)));

    cppcoro::sync_wait(fx.pool->cancel(id));

    std::optional<mutable_value> const stored
        = cppcoro::sync_wait(fx.store->get(job_record_key(id)));
    REQUIRE(stored.has_value());

    job_record const record{deserialize_job_record(*stored)};
    REQUIRE(record.status == job_status::succeeded);
}

TEST_CASE("local_pool cancel of an unknown id is a no-op", tag)
{
    pool_fixture fx;
    job_id const id{"no-such-id"};

    REQUIRE_NOTHROW(cppcoro::sync_wait(fx.pool->cancel(id)));

    std::optional<mutable_value> const stored
        = cppcoro::sync_wait(fx.store->get(job_record_key(id)));
    REQUIRE(!stored.has_value());
}
