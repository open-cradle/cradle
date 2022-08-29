#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include "../support/core.h"
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/service/core.h>

using namespace cradle;

namespace {

cppcoro::task<void>
eval_ptr2(immutable_cache_ptr<blob>& ptr2)
{
    co_await ptr2.task();
}

cppcoro::task<void>
eval_tasks(
    bool test_snapshots,
    inner_service_core& core,
    cppcoro::shared_task<blob> task0,
    cppcoro::shared_task<blob> task1)
{
    // Legend:
    // - record0 is the cache record for task0
    // - record1 is the cache record for task1
    // - record2 is the cache record for ptr2
    // - B is deep_sizeof(make_blob("42"))
    //
    // At this point, the eviction list is [record0, record1, record2]
    // record0 == {state: LOADING, size: 0}
    // record1 == {state: LOADING, size: 0}
    // record2 == {state: READY, size: B}

    size_t const B = deep_sizeof(make_blob("42"));
    auto& cache = core.inner_internals().cache;
    if (test_snapshots)
    {
        auto snapshot0 = get_cache_snapshot(cache);
        REQUIRE(snapshot0.pending_eviction.size() == 3);
        REQUIRE(snapshot0.total_size_eviction_list == B);
    }

    auto res1 = co_await task1;
    REQUIRE(res1 == make_blob("42"));
    // Now, the eviction list is [record0, record1, record2]
    // record0 == {state: LOADING, size: 0}
    // record1 == {state: READY, size: B}
    // record2 == {state: READY, size: B}

    if (test_snapshots)
    {
        auto snapshot1 = get_cache_snapshot(cache);
        REQUIRE(snapshot1.pending_eviction.size() == 3);
        REQUIRE(snapshot1.total_size_eviction_list == 2 * B);
    }

    // Simulate another thread kicking in and cleaning up the eviction list.
    // The clean-up iterates over the records and invalidates all of them.
    // Thanks to record1 and record2, record0 will be deleted even if the
    // total_size bookkeeping is wrong.
    clear_unused_entries(cache);

    if (test_snapshots)
    {
        auto snapshot2 = get_cache_snapshot(cache);
        REQUIRE(snapshot2.pending_eviction.size() == 0);
        REQUIRE(snapshot2.total_size_eviction_list == 0);
    }

    // The records have been deleted, but a reference to record0's key still
    // exists and will be passed to generic_disk_cached() when task0 is
    // evaluated. The framework must ensure it has captured the reference
    // somewhere or a crash will occur.
    auto res0 = co_await task0;
    REQUIRE(res0 == make_blob("42"));
}

void
do_the_test(bool clear_key0, bool test_snapshots)
{
    inner_service_core core;
    init_test_inner_service(core);

    auto create_task01
        = []() -> cppcoro::task<blob> { co_return make_blob("42"); };

    // Create a first cache record, zero size for now.
    captured_id key0{make_captured_id(0)};
    auto task0{fully_cached<blob>(core, key0, create_task01)};
    if (clear_key0)
    {
        // Ensure the only remaining reference to key0's id_interface object
        // is in the cache record.
        key0.clear();
    }

    // Create a second cache record, zero size for now.
    captured_id key1{make_captured_id(1)};
    auto task1{fully_cached<blob>(core, key1, create_task01)};

    auto create_task2 = [](captured_id const&) -> cppcoro::task<blob> {
        co_return make_blob("43");
    };
    {
        // Create a third cache record, with non-zero size.
        captured_id key2{make_captured_id(2)};
        immutable_cache_ptr<blob> ptr2(
            core.inner_internals().cache,
            key2,
            wrap_task_creator<blob>(create_task2));

        // Evaluating ptr2 makes the cache record READY and sets its size.
        cppcoro::sync_wait(eval_ptr2(ptr2));
        // ptr2's destructor moves the cache record to the eviction list.
    }

    cppcoro::sync_wait(eval_tasks(test_snapshots, core, task0, task1));
}

} // namespace

TEST_CASE("Clear eviction list during task evaluation", "[inner][service]")
{
    do_the_test(true, false);
}

TEST_CASE(
    "Consistent total_size when purging eviction list", "[inner][service]")
{
    do_the_test(false, true);
}
