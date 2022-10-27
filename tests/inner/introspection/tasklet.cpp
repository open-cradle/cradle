#include <chrono>
#include <thread>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../support/inner_service.h"
#include "../../support/tasklet_testing.h"
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_impl.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/service/resources.h>
//#include <cradle/plugins/disk_cache/serialization/native/native.h>

using namespace cradle;

// A newly created tasklet will be the latest one for which
// info can be retrieved.
tasklet_info
latest_tasklet_info()
{
    auto all_infos = get_tasklet_infos(true);
    REQUIRE(all_infos.size() > 0);
    return all_infos[all_infos.size() - 1];
}

TEST_CASE("create_tasklet_tracker", "[introspection]")
{
    clean_tasklet_admin_fixture fixture;

    auto t0 = create_tasklet_tracker("my_pool", "my_title");
    REQUIRE(t0 != nullptr);
    auto info0 = latest_tasklet_info();
    // Assume the clock has at least millisecond precision, causing
    // the two time_point's to differ
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto t1 = create_tasklet_tracker("other_pool", "other_title");
    REQUIRE(t1 != nullptr);
    REQUIRE(t1->own_id() != t0->own_id());
    auto info1 = latest_tasklet_info();

    // Test info0 (first tasklet)
    REQUIRE(info0.own_id() == t0->own_id());
    REQUIRE(info0.pool_name() == "my_pool");
    REQUIRE(info0.title() == "my_title");
    REQUIRE(!info0.have_client());
    auto events0 = info0.events();
    REQUIRE(events0.size() == 1);
    auto event00 = events0[0];
    REQUIRE(event00.what() == tasklet_event_type::SCHEDULED);
    REQUIRE(event00.details() == "");

    // Test info1 (second tasklet)
    REQUIRE(info1.own_id() == t1->own_id());
    REQUIRE(info1.pool_name() == "other_pool");
    REQUIRE(info1.title() == "other_title");
    REQUIRE(!info1.have_client());
    auto events1 = info1.events();
    REQUIRE(events1.size() == 1);
    auto event10 = events1[0];
    REQUIRE(event10.what() == tasklet_event_type::SCHEDULED);
    REQUIRE(event10.details() == "");

    // Test info0 versus info1
    REQUIRE(info1.own_id() != info0.own_id());
    // The system clock may not be monotonic, so cannot test >
    // Even != may be theoretically unsound.
    REQUIRE(event10.when() != event00.when());
}

TEST_CASE("create_tasklet_tracker with client", "[introspection]")
{
    clean_tasklet_admin_fixture fixture;

    auto client = create_tasklet_tracker("client_pool", "client_title");
    auto client_info = latest_tasklet_info();

    create_tasklet_tracker("my_pool", "my_title", client);
    auto my_info = latest_tasklet_info();

    REQUIRE(!client_info.have_client());
    REQUIRE(my_info.own_id() != client_info.own_id());
    REQUIRE(my_info.have_client());
    REQUIRE(my_info.client_id() == client_info.own_id());
}

TEST_CASE("tasklet_run", "[introspection]")
{
    clean_tasklet_admin_fixture fixture;

    auto me = create_tasklet_tracker("my_pool", "my_title");
    {
        tasklet_run run_tracker{me};
        auto events0 = latest_tasklet_info().events();
        REQUIRE(events0.size() == 2);
        REQUIRE(events0[0].what() == tasklet_event_type::SCHEDULED);
        REQUIRE(events0[1].what() == tasklet_event_type::RUNNING);
        REQUIRE(events0[1].details() == "");
    }
    auto events1 = latest_tasklet_info().events();
    REQUIRE(events1.size() == 3);
    REQUIRE(events1[0].what() == tasklet_event_type::SCHEDULED);
    REQUIRE(events1[1].what() == tasklet_event_type::RUNNING);
    REQUIRE(events1[1].details() == "");
    REQUIRE(events1[2].what() == tasklet_event_type::FINISHED);
    REQUIRE(events1[2].details() == "");
}

TEST_CASE("tasklet_await", "[introspection]")
{
    clean_tasklet_admin_fixture fixture;

    auto me = create_tasklet_tracker("my_pool", "my_title");
    tasklet_run run_tracker{me};
    {
        tasklet_await await_tracker{me, "awaiting...", make_id(87)};
        auto events0 = latest_tasklet_info().events();
        REQUIRE(events0.size() == 3);
        REQUIRE(events0[0].what() == tasklet_event_type::SCHEDULED);
        REQUIRE(events0[1].what() == tasklet_event_type::RUNNING);
        REQUIRE(events0[2].what() == tasklet_event_type::BEFORE_CO_AWAIT);
        REQUIRE(events0[2].details() == "awaiting... 87");
    }
    auto events1 = latest_tasklet_info().events();
    REQUIRE(events1.size() == 4);
    REQUIRE(events1[0].what() == tasklet_event_type::SCHEDULED);
    REQUIRE(events1[1].what() == tasklet_event_type::RUNNING);
    REQUIRE(events1[2].what() == tasklet_event_type::BEFORE_CO_AWAIT);
    REQUIRE(events1[2].details() == "awaiting... 87");
    REQUIRE(events1[3].what() == tasklet_event_type::AFTER_CO_AWAIT);
    REQUIRE(events1[3].details() == "");
}

TEST_CASE("shared_task_for_cacheable", "[introspection]")
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
