#include <sstream>
#include <stdexcept>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/caching/immutable.h>
#include <cradle/inner/caching/immutable/local_locked_record.h>
#include <cradle/inner/core/get_unique_string.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][caching][immutable]";

cppcoro::shared_task<void>
test_task(untyped_immutable_cache_ptr& untyped_ptr, int the_answer)
{
    using ptr_type = immutable_cache_ptr<int>;
    auto& ptr = static_cast<ptr_type&>(untyped_ptr);
    ptr.record_value(std::move(the_answer));
    co_return;
}

template<class Value>
Value
await_cache_value(immutable_cache_ptr<Value>& ptr)
{
    cppcoro::sync_wait(ptr.ensure_value_task());
    return ptr.get_value();
}

} // namespace

TEST_CASE("basic immutable cache usage", tag)
{
    std::string key0 = get_unique_string(make_id(0));
    std::string key1 = get_unique_string(make_id(1));

    immutable_cache cache{immutable_cache_config{1024}};
    {
        INFO("Cache reset() works as expected.");
        cache.reset(immutable_cache_config{1024});
    }

    INFO(
        "The first time an immutable_cache_ptr is attached to a new key, "
        "its :create_job callback is invoked.");
    bool p_needed_creation = false;
    auto p_key = make_captured_id(0);
    immutable_cache_ptr<int> p(
        cache, p_key, [&](untyped_immutable_cache_ptr& ptr) {
            p_needed_creation = true;
            return test_task(ptr, 42);
        });
    REQUIRE(p_needed_creation);
    // Also check that all the ptr accessors work.
    REQUIRE(p.is_loading());
    REQUIRE(!p.is_ready());
    REQUIRE(!p.is_failed());
    REQUIRE(p.key() == make_id(0));

    {
        INFO("get_cache_snapshot reflects that entry 0 is loading.");
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::LOADING, 0}}, {}}));
    }

    bool q_needed_creation = false;
    auto q_key = make_captured_id(1);
    auto q = std::make_unique<immutable_cache_ptr<int>>(
        cache, q_key, [&](untyped_immutable_cache_ptr& ptr) {
            q_needed_creation = true;
            return test_task(ptr, 112);
        });
    {
        INFO(
            "The first time an immutable_cache_ptr is attached to a new key, "
            "its :create_job callback is invoked.");
        REQUIRE(q_needed_creation);
    }
    // Also check all that all the ptr accessors work.
    REQUIRE(!q->is_ready());
    REQUIRE(q->key() == make_id(1));

    {
        INFO(
            "get_cache_snapshot reflects that there are two entries loading.");
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::LOADING, 0},
                 {key1, immutable_cache_entry_state::LOADING, 0}},
                {}}));
    }

    // p and r have the same id
    bool r_needed_creation = false;
    auto r_key = make_captured_id(0);
    immutable_cache_ptr<int> r(
        cache, r_key, [&](untyped_immutable_cache_ptr& ptr) {
            r_needed_creation = true;
            return test_task(ptr, 42);
        });
    {
        INFO(
            "The second time an immutable_cache_ptr is attached to a key, "
            "its :create_job callback is NOT invoked.");
        REQUIRE(!r_needed_creation);
    }

    {
        INFO("get_cache_snapshot shows no change.");
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::LOADING, 0},
                 {key1, immutable_cache_entry_state::LOADING, 0}},
                {}}));
    }

    {
        INFO(
            "When a cache pointer p is waited on, this triggers production of "
            "the value. The value is correctly received and reflected in the "
            "cache snapshot.");
        REQUIRE(await_cache_value(p) == 42);
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::READY, sizeof(int)},
                 {key1, immutable_cache_entry_state::LOADING, 0}},
                {}}));
        REQUIRE(p.is_ready());
        REQUIRE(q->is_loading());
        REQUIRE(r.is_ready());
    }

    {
        INFO(
            "Waiting on r (with the same id as p) gives the same result, "
            "but otherwise nothing changes.");
        REQUIRE(await_cache_value(r) == 42);
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::READY, sizeof(int)},
                 {key1, immutable_cache_entry_state::LOADING, 0}},
                {}}));
        REQUIRE(p.is_ready());
        REQUIRE(q->is_loading());
        REQUIRE(r.is_ready());
    }

    {
        INFO("Waiting on q (different id) gives the expected results.");
        REQUIRE(await_cache_value(*q) == 112);
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::READY, sizeof(int)},
                 {key1, immutable_cache_entry_state::READY, sizeof(int)}},
                {}}));
        REQUIRE(p.is_ready());
        REQUIRE(q->is_ready());
        REQUIRE(r.is_ready());
    }

    {
        INFO("Deleting q will put it into the eviction list.");

        q.reset();

        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::READY, sizeof(int)}},
                {{key1, immutable_cache_entry_state::READY, sizeof(int)}}}));
    }

    {
        INFO(
            "Clearing unused entries in the cache will clear out q's old "
            "value.");

        clear_unused_entries(cache);

        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::READY, sizeof(int)}},
                {}}));
    }
}

namespace {

cppcoro::shared_task<void>
one_kb_string_task(untyped_immutable_cache_ptr& untyped_ptr, char content)
{
    using ptr_type = immutable_cache_ptr<std::string>;
    auto& ptr = static_cast<ptr_type&>(untyped_ptr);
    std::string result(1024, content);
    ptr.record_value(std::move(result));
    co_return;
}

} // namespace

TEST_CASE("immutable cache LRU eviction", tag)
{
    // Initialize the cache with 1.5kB of space for unused data.
    immutable_cache cache(immutable_cache_config{1536});

    // Declare an interest in ID(1).
    bool p_needed_creation = false;
    auto p_key = make_captured_id(1);
    auto p = std::make_unique<immutable_cache_ptr<std::string>>(
        cache, p_key, [&](untyped_immutable_cache_ptr& ptr) {
            p_needed_creation = true;
            return one_kb_string_task(ptr, 'a');
        });
    REQUIRE(p_needed_creation);
    REQUIRE(await_cache_value(*p) == std::string(1024, 'a'));

    // Declare an interest in ID(2).
    bool q_needed_creation = false;
    auto q_key = make_captured_id(2);
    auto q = std::make_unique<immutable_cache_ptr<std::string>>(
        cache, q_key, [&](untyped_immutable_cache_ptr& ptr) {
            q_needed_creation = true;
            return one_kb_string_task(ptr, 'b');
        });
    REQUIRE(q_needed_creation);
    REQUIRE(await_cache_value(*q) == std::string(1024, 'b'));

    // Revoke interest in both IDs.
    // Since only one will fit in the cache, this should evict ID(1).
    p.reset();
    q.reset();

    // If we redeclare interest in ID(1), it should require creation.
    bool r_needed_creation = false;
    auto r_key = make_captured_id(1);
    immutable_cache_ptr<std::string> r(
        cache, r_key, [&](untyped_immutable_cache_ptr& ptr) {
            r_needed_creation = true;
            return one_kb_string_task(ptr, 'a');
        });
    REQUIRE(r_needed_creation);
    REQUIRE(!r.is_ready());
    REQUIRE(await_cache_value(r) == std::string(1024, 'a'));

    // If we redeclare interest in ID(2), it should NOT require creation.
    bool s_needed_creation = false;
    auto s_key = make_captured_id(2);
    immutable_cache_ptr<std::string> s(
        cache, s_key, [&](untyped_immutable_cache_ptr& ptr) {
            s_needed_creation = true;
            return one_kb_string_task(ptr, 'b');
        });
    REQUIRE(!s_needed_creation);
    REQUIRE(s.is_ready());
    REQUIRE(await_cache_value(s) == std::string(1024, 'b'));
}

TEST_CASE("immutable cache record locking", tag)
{
    immutable_cache cache(immutable_cache_config{1024});
    auto p_key = make_captured_id(1);
    auto q_key = make_captured_id(2);

    // Declare an interest in ID(1).
    bool p0_needed_creation = false;
    auto p0 = std::make_unique<immutable_cache_ptr<int>>(
        cache, p_key, [&](untyped_immutable_cache_ptr& ptr) {
            p0_needed_creation = true;
            return test_task(ptr, 111);
        });
    REQUIRE(p0_needed_creation);
    REQUIRE(await_cache_value(*p0) == 111);

    // Declare an interest in ID(2).
    bool q0_needed_creation = false;
    auto q0 = std::make_unique<immutable_cache_ptr<int>>(
        cache, q_key, [&](untyped_immutable_cache_ptr& ptr) {
            q0_needed_creation = true;
            return test_task(ptr, 222);
        });
    REQUIRE(q0_needed_creation);
    REQUIRE(await_cache_value(*q0) == 222);

    // Lock the cache record for ID(2).
    auto q_lock = std::make_unique<cache_record_lock>();
    q_lock->set_record(
        std::make_unique<local_locked_cache_record>(q0->get_record()));

    auto info0a{get_summary_info(cache)};
    CHECK(info0a.ac_num_records_in_use == 2);
    CHECK(info0a.ac_num_records_pending_eviction == 0);
    CHECK(info0a.cas_num_records == 2);
    CHECK(info0a.cas_total_size == 2 * sizeof(int));
    CHECK(info0a.cas_total_locked_size == sizeof(int));
    CHECK(info0a.hit_count == 0);
    CHECK(info0a.miss_count == 2);

    // Revoke interest in both IDs.
    // This should make ID(1) pending-eviction, but not ID(2) due to the lock.
    p0.reset();
    q0.reset();
    auto info0b{get_summary_info(cache)};
    CHECK(info0b.ac_num_records_in_use == 1);
    CHECK(info0b.ac_num_records_pending_eviction == 1);
    CHECK(info0b.cas_num_records == 2);
    CHECK(info0b.cas_total_size == 2 * sizeof(int));
    CHECK(info0b.cas_total_locked_size == sizeof(int));
    CHECK(info0b.hit_count == 0);
    CHECK(info0b.miss_count == 2);

    // Purging the eviction entries should remove ID(1) but not ID(2).
    clear_unused_entries(cache);
    auto info0c{get_summary_info(cache)};
    CHECK(info0c.ac_num_records_in_use == 1);
    CHECK(info0c.ac_num_records_pending_eviction == 0);
    CHECK(info0c.cas_num_records == 1);
    CHECK(info0c.cas_total_size == sizeof(int));
    CHECK(info0c.cas_total_locked_size == sizeof(int));
    CHECK(info0c.hit_count == 0);
    CHECK(info0c.miss_count == 2);

    // If we redeclare interest in ID(1), it should require creation.
    bool p1_needed_creation = false;
    auto p1 = std::make_unique<immutable_cache_ptr<int>>(
        cache, p_key, [&](untyped_immutable_cache_ptr& ptr) {
            p1_needed_creation = true;
            return test_task(ptr, 111);
        });
    REQUIRE(p1_needed_creation);
    REQUIRE(await_cache_value(*p1) == 111);

    // If we redeclare interest in ID(2), it should NOT require creation.
    bool q1_needed_creation = false;
    auto q1 = std::make_unique<immutable_cache_ptr<int>>(
        cache, q_key, [&](untyped_immutable_cache_ptr& ptr) {
            q1_needed_creation = true;
            return test_task(ptr, 222);
        });
    REQUIRE(!q1_needed_creation);
    REQUIRE(await_cache_value(*q1) == 222);

    auto info1a{get_summary_info(cache)};
    CHECK(info1a.ac_num_records_in_use == 2);
    CHECK(info1a.ac_num_records_pending_eviction == 0);
    CHECK(info1a.cas_num_records == 2);
    CHECK(info1a.cas_total_size == 2 * sizeof(int));
    CHECK(info1a.cas_total_locked_size == sizeof(int));
    CHECK(info1a.hit_count == 1);
    CHECK(info1a.miss_count == 3);

    // Remove the lock and revoke interest in both IDs.
    // No entry remains in use.
    q_lock.reset();
    p1.reset();
    q1.reset();
    auto info1b{get_summary_info(cache)};
    CHECK(info1b.ac_num_records_in_use == 0);
    CHECK(info1b.ac_num_records_pending_eviction == 2);
    CHECK(info1b.cas_num_records == 2);
    CHECK(info1b.cas_total_size == 2 * sizeof(int));
    CHECK(info1b.cas_total_locked_size == 0);
    CHECK(info1b.hit_count == 1);
    CHECK(info1b.miss_count == 3);

    // All entries can be removed from the cache.
    clear_unused_entries(cache);
    auto info1c{get_summary_info(cache)};
    CHECK(info1c.ac_num_records_in_use == 0);
    CHECK(info1c.ac_num_records_pending_eviction == 0);
    CHECK(info1c.cas_num_records == 0);
    CHECK(info1c.cas_total_size == 0);
    CHECK(info1c.cas_total_locked_size == 0);
    CHECK(info1c.hit_count == 1);
    CHECK(info1c.miss_count == 3);
}

TEST_CASE("immutable cache - record locking with delayed task run", tag)
{
    immutable_cache cache(immutable_cache_config{1024});
    auto p_key = make_captured_id(1);
    immutable_cache_ptr<int> p{
        cache, p_key, [&](untyped_immutable_cache_ptr& ptr) {
            return test_task(ptr, 9);
        }};
    cache_record_lock p_lock;
    p_lock.set_record(
        std::make_unique<local_locked_cache_record>(p.get_record()));

    // The task has not run, so there is no CAS record yet.
    auto info0{get_summary_info(cache)};
    CHECK(info0.ac_num_records_in_use == 1);
    CHECK(info0.ac_num_records_pending_eviction == 0);
    CHECK(info0.cas_num_records == 0);
    CHECK(info0.cas_total_size == 0);
    CHECK(info0.cas_total_locked_size == 0);

    REQUIRE(await_cache_value(p) == 9);

    auto info1{get_summary_info(cache)};
    CHECK(info1.ac_num_records_in_use == 1);
    CHECK(info1.ac_num_records_pending_eviction == 0);
    CHECK(info1.cas_num_records == 1);
    CHECK(info1.cas_total_size == sizeof(int));
    CHECK(info1.cas_total_locked_size == sizeof(int));
}

TEST_CASE("immutable cache - re-obtain lock", tag)
{
    immutable_cache cache(immutable_cache_config{1024});
    auto p_key = make_captured_id(1);
    immutable_cache_ptr<int> p(
        cache, p_key, [&](untyped_immutable_cache_ptr& ptr) {
            return test_task(ptr, 1);
        });

    cache_record_lock p_lock;
    REQUIRE_NOTHROW(p_lock.set_record(
        std::make_unique<local_locked_cache_record>(p.get_record())));
    REQUIRE_THROWS_AS(
        p_lock.set_record(
            std::make_unique<local_locked_cache_record>(p.get_record())),
        std::logic_error);
}

namespace detail {

// Similar to resolve_request.h code, but not using requests.
// Maybe there is a common abstraction that should be factored out.
template<typename Value>
cppcoro::task<Value>
eval_ptr(std::unique_ptr<immutable_cache_ptr<Value>> ptr)
{
    co_await ptr->ensure_value_task();
    co_return ptr->get_value();
}

template<typename Value, typename CreateTask>
cppcoro::shared_task<void>
eval_created_task(CreateTask create_task, immutable_cache_ptr<Value>& ptr)
{
    try
    {
        ptr.record_value(co_await create_task());
    }
    catch (...)
    {
        ptr.record_failure();
        throw;
    }
}

template<typename Value, typename CreateTask>
cppcoro::task<Value>
eval_cached(immutable_cache& cache, captured_id key, CreateTask create_task)
{
    using ptr_type = immutable_cache_ptr<Value>;
    auto ptr{std::make_unique<ptr_type>(
        cache,
        key,
        [create_task
         = std::move(create_task)](untyped_immutable_cache_ptr& ptr) {
            return eval_created_task<Value>(
                create_task, static_cast<ptr_type&>(ptr));
        })};
    return eval_ptr(std::move(ptr));
}

} // namespace detail

TEST_CASE("two-phase memory caching", tag)
{
    immutable_cache cache{immutable_cache_config{1024}};

    auto key0 = make_captured_id("task0");
    auto task0 = ::detail::eval_cached<int>(
        cache, key0, []() -> cppcoro::task<int> { co_return 3 * 3; });
    REQUIRE(cppcoro::sync_wait(task0) == 9);
    // Expecting one AC record in use, one CAS record
    auto info0{get_summary_info(cache)};
    REQUIRE(info0.ac_num_records_in_use == 1);
    REQUIRE(info0.ac_num_records_pending_eviction == 0);
    REQUIRE(info0.cas_num_records == 1);
    REQUIRE(info0.cas_total_size == sizeof(int));

    // Different key, but same result as task0
    auto key1 = make_captured_id("task1");
    auto task1 = ::detail::eval_cached<int>(
        cache, key1, []() -> cppcoro::task<int> { co_return 2 + 7; });
    REQUIRE(cppcoro::sync_wait(task1) == 9);
    // Expecting two AC records in use, sharing the one CAS record
    auto info1{get_summary_info(cache)};
    REQUIRE(info1.ac_num_records_in_use == 2);
    REQUIRE(info1.ac_num_records_pending_eviction == 0);
    REQUIRE(info1.cas_num_records == 1);
    REQUIRE(info1.cas_total_size == sizeof(int));

    // Different key, different result
    auto key2 = make_captured_id("task2");
    auto task2 = ::detail::eval_cached<int>(
        cache, key2, []() -> cppcoro::task<int> { co_return 1 + 2; });
    REQUIRE(cppcoro::sync_wait(task2) == 3);
    // Expecting three AC records in use, two CAS records
    auto info2{get_summary_info(cache)};
    REQUIRE(info2.ac_num_records_in_use == 3);
    REQUIRE(info2.ac_num_records_pending_eviction == 0);
    REQUIRE(info2.cas_num_records == 2);
    REQUIRE(info2.cas_total_size == 2 * sizeof(int));

    // Move task0's record to the eviction list
    task0 = decltype(task0)();
    // Expecting two AC records in use, still two CAS records
    auto info3{get_summary_info(cache)};
    REQUIRE(info3.ac_num_records_in_use == 2);
    REQUIRE(info3.ac_num_records_pending_eviction == 1);
    REQUIRE(info3.cas_num_records == 2);
    REQUIRE(info3.cas_total_size == 2 * sizeof(int));

    // Move task1's record to the eviction list
    task1 = decltype(task1)();
    // Expecting one AC record in use, still two CAS records
    // (one of them being referenced only from the eviction list)
    auto info4{get_summary_info(cache)};
    REQUIRE(info4.ac_num_records_in_use == 1);
    REQUIRE(info4.ac_num_records_pending_eviction == 2);
    REQUIRE(info4.cas_num_records == 2);
    REQUIRE(info4.cas_total_size == 2 * sizeof(int));

    // Purge the eviction list, deleting two AC records and one CAS record
    clear_unused_entries(cache);
    // Expecting one AC record in use, one CAS record
    auto info5{get_summary_info(cache)};
    REQUIRE(info5.ac_num_records_in_use == 1);
    REQUIRE(info5.ac_num_records_pending_eviction == 0);
    REQUIRE(info5.cas_num_records == 1);
    REQUIRE(info5.cas_total_size == sizeof(int));

    // Move task2's record to the eviction list
    task2 = decltype(task2)();
    // Expecting no AC record in use, still one CAS record
    auto info6{get_summary_info(cache)};
    REQUIRE(info6.ac_num_records_in_use == 0);
    REQUIRE(info6.ac_num_records_pending_eviction == 1);
    REQUIRE(info6.cas_num_records == 1);
    REQUIRE(info6.cas_total_size == sizeof(int));

    // Purge the eviction list, deleting the last AC record and CAS record
    clear_unused_entries(cache);
    // Expecting no AC record in use, no CAS record
    auto info7{get_summary_info(cache)};
    REQUIRE(info7.ac_num_records_in_use == 0);
    REQUIRE(info7.ac_num_records_pending_eviction == 0);
    REQUIRE(info7.cas_num_records == 0);
    REQUIRE(info7.cas_total_size == 0);
}

TEST_CASE("immutable cache - snapshotting", tag)
{
    immutable_cache cache{immutable_cache_config{1024}};

    // Create two AC records, one CAS record
    auto key0 = make_captured_id("task0");
    auto task0 = ::detail::eval_cached<int>(
        cache, key0, []() -> cppcoro::task<int> { co_return 3 * 3; });
    REQUIRE(cppcoro::sync_wait(task0) == 9);
    auto key1 = make_captured_id("task1");
    auto task1 = ::detail::eval_cached<int>(
        cache, key1, []() -> cppcoro::task<int> { co_return 2 + 7; });
    REQUIRE(cppcoro::sync_wait(task1) == 9);

    auto snapshot0{get_cache_snapshot(cache)};
    CHECK(snapshot0.in_use.size() == 2);
    CHECK(snapshot0.pending_eviction.size() == 0);
    CHECK(snapshot0 == snapshot0);
    std::ostringstream oss0;
    oss0 << snapshot0;
    auto str0{oss0.str()};
    CHECK(str0.find("2 entries in use") != str0.npos);
    CHECK(str0.find("0 entries pending eviction") != str0.npos);

    // Move task0's record to the eviction list
    task0 = decltype(task0)();

    auto snapshot1{get_cache_snapshot(cache)};
    CHECK(snapshot1.in_use.size() == 1);
    CHECK(snapshot1.pending_eviction.size() == 1);
    CHECK(snapshot1 == snapshot1);
    std::ostringstream oss1;
    oss1 << snapshot1;
    auto str1{oss1.str()};
    CHECK(str1.find("1 entries in use") != str1.npos);
    CHECK(str1.find("1 entries pending eviction") != str1.npos);

    CHECK(snapshot0 != snapshot1);
    CHECK(!(snapshot0 == snapshot1));
}

TEST_CASE("immutable caching - retry failure", tag)
{
    class forced_failure : public std::runtime_error
    {
     public:
        using std::runtime_error::runtime_error;
    };

    immutable_cache cache{immutable_cache_config{1024}};
    auto key = make_captured_id("task");

    // A failing task causes an AC record with state "failed" and no CAS
    // record.
    auto task0
        = ::detail::eval_cached<int>(cache, key, []() -> cppcoro::task<int> {
              throw forced_failure{"forced failure"};
          });
    REQUIRE_THROWS_AS(cppcoro::sync_wait(task0), forced_failure);
    auto snapshot0{get_cache_snapshot(cache)};
    CHECK(snapshot0.in_use.size() == 1);
    CHECK(snapshot0.in_use[0].state == immutable_cache_entry_state::FAILED);
    CHECK(snapshot0.pending_eviction.size() == 0);
    auto info0{get_summary_info(cache)};
    CHECK(info0.cas_total_size == 0);

    // Retry the failed task, and let it succeed: the existing AC record
    // becomes "ready", and the result is stored as a CAS record.
    auto task1 = ::detail::eval_cached<int>(
        cache, key, []() -> cppcoro::task<int> { co_return 9; });
    REQUIRE(cppcoro::sync_wait(task1) == 9);
    auto snapshot1{get_cache_snapshot(cache)};
    CHECK(snapshot1.in_use.size() == 1);
    CHECK(snapshot1.in_use[0].state == immutable_cache_entry_state::READY);
    CHECK(snapshot1.pending_eviction.size() == 0);
    auto info1{get_summary_info(cache)};
    CHECK(info1.cas_total_size == sizeof(int));
}
