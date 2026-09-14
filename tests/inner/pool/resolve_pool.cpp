#include <cradle/inner/resolve/resolve_pool.h>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include "../../support/inner_service.h"
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/pool/job_record.h>
#include <cradle/inner/pool/job_spec.h>
#include <cradle/inner/pool/job_types.h>
#include <cradle/inner/pool/pool_intf.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/content_helpers.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/memory_ac.h>
#include <cradle/inner/storage/memory_cas.h>
#include <cradle/inner/storage/memory_mutable_store.h>
#include <cradle/inner/storage/mutable_store_intf.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][pool][resolve]";

// In-test pool double that refers to real in-memory M1 stores it does not own.
// submit() records a per-job record (via serialize_job_record) into the
// mutable store; the status and any output digest come from a caller-supplied
// prototype. A submit counter lets a test prove the AC-hit short-circuit
// never dispatched a job. Entirely inner-lane; no plugins dependency.
class test_pool : public pool_intf
{
 public:
    test_pool(
        std::string name,
        cas_intf& cas,
        ac_intf& ac,
        mutable_store_intf& store)
        : name_{std::move(name)}, cas_{cas}, ac_{ac}, store_{store}
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
        return cas_;
    }

    ac_intf&
    ac() override
    {
        return ac_;
    }

    mutable_store_intf&
    mutable_store() override
    {
        return store_;
    }

    cppcoro::task<job_id>
    submit(job_spec spec) override
    {
        ++submit_count;
        job_id const id{"job-1"};
        job_record record{record_to_write};
        record.id = id;
        record.key = spec.key;
        record.pool_name = name_;
        co_await store_.put(job_record_key(id), serialize_job_record(record));
        co_return id;
    }

    cppcoro::task<void>
    cancel(job_id) override
    {
        co_return;
    }

    int submit_count{0};
    job_record record_to_write;

 private:
    std::string name_;
    cas_intf& cas_;
    ac_intf& ac_;
    mutable_store_intf& store_;
};

// In-memory mutable store that returns a non-terminal record for the observed
// key on its first polls and only a terminal record after a configured number
// of reads. It lets a test prove the observe loop iterates and terminates,
// exercising the context's schedule_after between polls.
class polling_mutable_store : public mutable_store_intf
{
 public:
    polling_mutable_store(
        std::string name,
        std::string key,
        mutable_value non_terminal,
        mutable_value terminal,
        int gets_before_terminal)
        : name_{std::move(name)},
          key_{std::move(key)},
          non_terminal_{std::move(non_terminal)},
          terminal_{std::move(terminal)},
          gets_before_terminal_{gets_before_terminal}
    {
    }

    std::string const&
    name() const override
    {
        return name_;
    }

    cppcoro::task<void>
    put(std::string, mutable_value) override
    {
        co_return;
    }

    cppcoro::task<std::optional<mutable_value>>
    get(std::string key) override
    {
        if (key == key_)
        {
            if (get_count_ < gets_before_terminal_)
            {
                ++get_count_;
                co_return non_terminal_;
            }
            co_return terminal_;
        }
        co_return std::nullopt;
    }

    cppcoro::task<bool>
    exists(std::string key) override
    {
        co_return key == key_;
    }

    int
    get_count() const
    {
        return get_count_;
    }

 private:
    std::string name_;
    std::string key_;
    mutable_value non_terminal_;
    mutable_value terminal_;
    int gets_before_terminal_;
    int get_count_{0};
};

} // namespace

TEST_CASE("resolve_leaf_on_pool short-circuits on an AC hit", tag)
{
    // The pool's AC already associates the key with a digest whose content is
    // in the CAS. resolve_leaf_on_pool must return that content without ever
    // submitting a job.
    auto resources = make_inner_test_resources();
    non_caching_request_resolution_context ctx{*resources};

    auto cas = make_memory_cas("pool_cas");
    auto ac = make_memory_ac("pool_ac");
    auto store = make_memory_mutable_store("pool_store");
    test_pool pool{"local", *cas, *ac, *store};

    blob const content = make_blob("cached-result");
    digest const d = cppcoro::sync_wait(put_content(*cas, content));
    request_key const key = "leaf/ac-hit";
    cppcoro::sync_wait(ac->put(key, d));

    blob const result = cppcoro::sync_wait(resolve_leaf_on_pool(
        ctx, pool, "provider", key, "", std::vector<blob>{}, 0));

    REQUIRE(to_string(result) == "cached-result");
    REQUIRE(pool.submit_count == 0);
}

TEST_CASE(
    "resolve_leaf_on_pool classifies inputs, submits, and returns on success",
    tag)
{
    // No AC hit and an above-boundary input: the input's content must be
    // placed in the pool CAS (digest classification), submit must be called
    // once, and the succeeded result content must be returned.
    auto resources = make_inner_test_resources();
    non_caching_request_resolution_context ctx{*resources};

    auto cas = make_memory_cas("pool_cas");
    auto ac = make_memory_ac("pool_ac");
    auto store = make_memory_mutable_store("pool_store");
    test_pool pool{"local", *cas, *ac, *store};

    blob const result = make_blob("computed-result");
    digest const result_digest = cppcoro::sync_wait(put_content(*cas, result));
    pool.record_to_write.status = job_status::succeeded;
    pool.record_to_write.output_digest = result_digest;

    blob const big_input = make_blob("an input above the size boundary");
    std::size_t const boundary = 4;

    blob const returned = cppcoro::sync_wait(resolve_leaf_on_pool(
        ctx,
        pool,
        "provider",
        "leaf/submit",
        "",
        std::vector<blob>{big_input},
        boundary));

    // Digest classification placed the input's content in the pool CAS.
    REQUIRE(cppcoro::sync_wait(cas->exists(digest_of(big_input))));
    REQUIRE(pool.submit_count == 1);
    REQUIRE(to_string(returned) == "computed-result");
}

TEST_CASE("resolve_leaf_on_pool returns the succeeded result by digest", tag)
{
    // A succeeded record names the result by output_digest; the exact CAS
    // content for that digest must be returned.
    auto resources = make_inner_test_resources();
    non_caching_request_resolution_context ctx{*resources};

    auto cas = make_memory_cas("pool_cas");
    auto ac = make_memory_ac("pool_ac");
    auto store = make_memory_mutable_store("pool_store");
    test_pool pool{"local", *cas, *ac, *store};

    blob const result = make_blob("by-digest-result");
    digest const result_digest = cppcoro::sync_wait(put_content(*cas, result));
    pool.record_to_write.status = job_status::succeeded;
    pool.record_to_write.output_digest = result_digest;

    blob const returned = cppcoro::sync_wait(resolve_leaf_on_pool(
        ctx,
        pool,
        "provider",
        "leaf/by-digest",
        "",
        std::vector<blob>{make_blob("in")},
        1024));

    REQUIRE(to_string(returned) == "by-digest-result");
}

TEST_CASE("resolve_leaf_on_pool surfaces a failed job's error", tag)
{
    auto resources = make_inner_test_resources();
    non_caching_request_resolution_context ctx{*resources};

    auto cas = make_memory_cas("pool_cas");
    auto ac = make_memory_ac("pool_ac");
    auto store = make_memory_mutable_store("pool_store");
    test_pool pool{"local", *cas, *ac, *store};

    pool.record_to_write.status = job_status::failed;
    pool.record_to_write.error = "provider raised an error";

    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(resolve_leaf_on_pool(
            ctx,
            pool,
            "provider",
            "leaf/failed",
            "",
            std::vector<blob>{},
            1024)),
        async_error);
}

TEST_CASE("resolve_leaf_on_pool surfaces a cancelled job", tag)
{
    auto resources = make_inner_test_resources();
    non_caching_request_resolution_context ctx{*resources};

    auto cas = make_memory_cas("pool_cas");
    auto ac = make_memory_ac("pool_ac");
    auto store = make_memory_mutable_store("pool_store");
    test_pool pool{"local", *cas, *ac, *store};

    pool.record_to_write.status = job_status::cancelled;

    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(resolve_leaf_on_pool(
            ctx,
            pool,
            "provider",
            "leaf/cancelled",
            "",
            std::vector<blob>{},
            1024)),
        async_cancelled);
}

TEST_CASE("observe_until_terminal polls until a terminal record appears", tag)
{
    // The record is non-terminal on the first polls and only terminal after
    // several reads. observe_until_terminal must keep polling (exercising
    // schedule_after) and then return the terminal record.
    auto resources = make_inner_test_resources();
    non_caching_request_resolution_context ctx{*resources};

    job_id const id{"job-backoff"};

    job_record queued;
    queued.id = id;
    queued.key = "leaf/backoff";
    queued.pool_name = "local";
    queued.status = job_status::queued;

    job_record succeeded{queued};
    succeeded.status = job_status::succeeded;
    succeeded.output_digest = "digest-of-result";

    int const polls_before_terminal = 3;
    polling_mutable_store store{
        "polling_store",
        job_record_key(id),
        serialize_job_record(queued),
        serialize_job_record(succeeded),
        polls_before_terminal};

    auto cas = make_memory_cas("pool_cas");
    auto ac = make_memory_ac("pool_ac");
    test_pool pool{"local", *cas, *ac, store};

    job_record const record
        = cppcoro::sync_wait(observe_until_terminal(ctx, pool, id));

    REQUIRE(is_terminal(record.status));
    REQUIRE(record.status == job_status::succeeded);
    REQUIRE(record.output_digest == "digest-of-result");
    // The loop iterated past the non-terminal polls before terminating.
    REQUIRE(store.get_count() == polls_before_terminal);
}

TEST_CASE("a request naming no pool resolves inline", tag)
{
    // The no-pool path is unchanged: a plain request resolves to its value
    // without entering the job branch. Full end-to-end pool selection is
    // covered elsewhere.
    auto resources = make_inner_test_resources();
    non_caching_request_resolution_context ctx{*resources};

    auto req = rq_value(42);
    auto const value = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(value == 42);
}
