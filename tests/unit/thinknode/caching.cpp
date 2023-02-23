#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include "../../support/concurrency_testing.h"
#include "../../support/inner_service.h"
#include "../../support/tasklet_testing.h"
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/service/resources.h>
#include <cradle/thinknode/caching.h>
#include <cradle/typing/service/core.h>

using namespace cradle;

namespace {

static char const tag[] = "[unit][thinknode][caching]";

// A newly created tasklet will be the latest one for which
// info can be retrieved.
tasklet_info
latest_tasklet_info()
{
    auto all_infos = get_tasklet_infos(true);
    REQUIRE(all_infos.size() > 0);
    return all_infos[all_infos.size() - 1];
}

cppcoro::task<void>
eval_ptr2(immutable_cache_ptr<blob>& ptr2)
{
    co_await ptr2.task();
}

cppcoro::task<void>
eval_tasks(
    bool test_snapshots,
    inner_resources& resources,
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
    auto& cache = resources.memory_cache();
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

// TODO adapt to reply on inner constructs only and move to tests/inner
void
do_the_test(bool clear_key0, bool test_snapshots)
{
    inner_resources resources;
    init_test_inner_service(resources);

    auto create_task01
        = []() -> cppcoro::task<blob> { co_return make_blob("42"); };

    // Create a first cache record, zero size for now.
    captured_id key0{make_captured_id(0)};
    auto task0{fully_cached<blob>(resources, key0, create_task01)};
    if (clear_key0)
    {
        // Ensure the only remaining reference to key0's id_interface object
        // is in the cache record.
        key0.clear();
    }

    // Create a second cache record, zero size for now.
    captured_id key1{make_captured_id(1)};
    auto task1{fully_cached<blob>(resources, key1, create_task01)};

    auto create_task2 = [](captured_id const&) -> cppcoro::task<blob> {
        co_return make_blob("43");
    };
    {
        // Create a third cache record, with non-zero size.
        captured_id key2{make_captured_id(2)};
        immutable_cache_ptr<blob> ptr2(
            resources.memory_cache(),
            key2,
            wrap_task_creator<blob>(create_task2));

        // Evaluating ptr2 makes the cache record READY and sets its size.
        cppcoro::sync_wait(eval_ptr2(ptr2));
        // ptr2's destructor moves the cache record to the eviction list.
    }

    cppcoro::sync_wait(eval_tasks(test_snapshots, resources, task0, task1));
}

} // namespace

TEST_CASE("Clear eviction list during task evaluation", tag)
{
    do_the_test(true, false);
}

TEST_CASE("Consistent total_size when purging eviction list", tag)
{
    do_the_test(false, true);
}

TEST_CASE("small value disk caching", tag)
{
    service_core core;
    init_test_service(core);

    int execution_count = 0;
    auto counted_task = [&](int answer) -> cppcoro::task<dynamic> {
        ++execution_count;
        co_return dynamic(integer(answer));
    };

    {
        auto key = make_captured_id("id_12");
        auto result = secondary_cached<dynamic>(
            core, key, [&] { return counted_task(12); });
        REQUIRE(cppcoro::sync_wait(result) == dynamic(integer(12)));
        REQUIRE(execution_count == 1);
    }
    {
        auto key = make_captured_id("id_42");
        auto result = secondary_cached<dynamic>(
            core, key, [&] { return counted_task(42); });
        REQUIRE(cppcoro::sync_wait(result) == dynamic(integer(42)));
        REQUIRE(execution_count == 2);
    }
    // Data is written to the disk cache in a background thread, so we need to
    // wait for that to finish.
    sync_wait_write_disk_cache(core);
    // Now redo the 'id_12' task to see that it's not actually rerun.
    {
        auto key = make_captured_id("id_12");
        auto result = secondary_cached<dynamic>(
            core, key, [&] { return counted_task(12); });
        REQUIRE(cppcoro::sync_wait(result) == dynamic(integer(12)));
        REQUIRE(execution_count == 2);
    }
}

TEST_CASE("large value disk caching", tag)
{
    service_core core;
    init_test_service(core);

    auto generate_random_data = [](uint32_t seed) {
        std::vector<integer> result;
        std::minstd_rand eng(seed);
        std::uniform_int_distribution<integer> dist(0, 0x1'0000'0000);
        for (int i = 0; i < 256; ++i)
            result.push_back(dist(eng));
        return result;
    };

    int execution_count = 0;
    auto counted_task = [&](uint32_t seed) -> cppcoro::task<dynamic> {
        ++execution_count;
        co_return to_dynamic(generate_random_data(seed));
    };

    {
        auto key = make_captured_id("id_12");
        auto result = secondary_cached<dynamic>(
            core, key, [&] { return counted_task(12); });
        REQUIRE(
            cppcoro::sync_wait(result)
            == to_dynamic(generate_random_data(12)));
        REQUIRE(execution_count == 1);
    }
    {
        auto key = make_captured_id("id_42");
        auto result = secondary_cached<dynamic>(
            core, key, [&] { return counted_task(42); });
        REQUIRE(
            cppcoro::sync_wait(result)
            == to_dynamic(generate_random_data(42)));
        REQUIRE(execution_count == 2);
    }
    // Data is written to the disk cache in a background thread, so we need to
    // wait for that to finish.
    sync_wait_write_disk_cache(core);
    // Now redo the 'id_12' task to see that it's not actually rerun.
    {
        auto key = make_captured_id("id_12");
        auto result = secondary_cached<dynamic>(
            core, key, [&] { return counted_task(12); });
        REQUIRE(
            cppcoro::sync_wait(result)
            == to_dynamic(generate_random_data(12)));
        REQUIRE(execution_count == 2);
    }
}

TEST_CASE("cached tasks", tag)
{
    service_core core;
    init_test_service(core);

    int execution_count = 0;
    auto counted_task = [&](int answer) -> cppcoro::task<integer> {
        ++execution_count;
        co_return integer(answer);
    };

    {
        auto result = cached<integer>(
            core, make_captured_id(12), [&](captured_id const&) {
                return counted_task(12);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(12));
        REQUIRE(execution_count == 1);
    }
    {
        auto result = cached<integer>(
            core, make_captured_id(42), [&](captured_id const&) {
                return counted_task(42);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(42));
        REQUIRE(execution_count == 2);
    }
    // Now redo the '12' task to see that it's not actually rerun.
    {
        auto result = cached<integer>(
            core, make_captured_id(12), [&](captured_id const&) {
                return counted_task(12);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(12));
        REQUIRE(execution_count == 2);
    }
}

TEST_CASE("lazily generated cached tasks", tag)
{
    service_core core;
    init_test_service(core);

    int execution_count = 0;
    auto counted_task = [&](int answer) -> cppcoro::task<integer> {
        ++execution_count;
        co_return integer(answer);
    };

    {
        auto result = cached<integer>(
            core,
            make_captured_id(12),
            [&](captured_id const&) -> cppcoro::task<integer> {
                return counted_task(12);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(12));
        REQUIRE(execution_count == 1);
    }
    {
        auto result = cached<integer>(
            core,
            make_captured_id(42),
            [&](captured_id const&) -> cppcoro::task<integer> {
                return counted_task(42);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(42));
        REQUIRE(execution_count == 2);
    }
    // Now redo the '12' task to see that it's not actually rerun.
    {
        auto result = cached<integer>(
            core,
            make_captured_id(12),
            [&](captured_id const&) -> cppcoro::task<integer> {
                return counted_task(12);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(12));
        REQUIRE(execution_count == 2);
    }
}

TEST_CASE("shared_task_for_cacheable", tag)
{
    clean_tasklet_admin_fixture fixture;
    inner_resources resources;
    init_test_inner_service(resources);

    captured_id cache_key{make_captured_id(87)};
    auto task_creator = []() -> cppcoro::task<blob> {
        co_return make_blob(std::string("314"));
    };
    auto client = create_tasklet_tracker("client_pool", "client_title");
    auto me = make_shared_task_for_cacheable<blob>(
        resources, std::move(cache_key), task_creator, client, "my summary");
    auto res = cppcoro::sync_wait(
        [&]() -> cppcoro::task<blob> { co_return co_await me; }());

    REQUIRE(res == make_blob(std::string("314")));
    auto events = latest_tasklet_info().events();
    REQUIRE(events.size() == 3);
    REQUIRE(events[0].what() == tasklet_event_type::SCHEDULED);
    REQUIRE(events[1].what() == tasklet_event_type::BEFORE_CO_AWAIT);
    REQUIRE(events[1].details() == "my summary 87");
    REQUIRE(events[2].what() == tasklet_event_type::AFTER_CO_AWAIT);
}
