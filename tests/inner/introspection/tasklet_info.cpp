#include <catch2/catch.hpp>

#include "../../support/inner_service.h"
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_impl.h>
#include <cradle/inner/introspection/tasklet_info.h>

using namespace cradle;

static char const tag[] = "[introspection]";

TEST_CASE("tasklet_event", tag)
{
    for (int i = 0; i < num_tasklet_event_types; ++i)
    {
        auto what = static_cast<tasklet_event_type>(i);
        tasklet_event me{what};

        REQUIRE(me.when().time_since_epoch().count() > 0);
        REQUIRE(me.what() == what);
        REQUIRE(me.details() == "");
    }
}

TEST_CASE("tasklet_event with details", tag)
{
    for (int i = 0; i < num_tasklet_event_types; ++i)
    {
        auto what = static_cast<tasklet_event_type>(i);
        tasklet_event me{what, "my details"};

        REQUIRE(me.when().time_since_epoch().count() > 0);
        REQUIRE(me.what() == what);
        REQUIRE(me.details() == "my details");
    }
}

TEST_CASE("tasklet_event_type to string", tag)
{
    REQUIRE(to_string(tasklet_event_type::SCHEDULED) == "scheduled");
    REQUIRE(to_string(tasklet_event_type::RUNNING) == "running");
    REQUIRE(
        to_string(tasklet_event_type::BEFORE_CO_AWAIT) == "before co_await");
    REQUIRE(to_string(tasklet_event_type::AFTER_CO_AWAIT) == "after co_await");
    REQUIRE(to_string(tasklet_event_type::FINISHED) == "finished");
    REQUIRE(to_string(tasklet_event_type::UNKNOWN) == "unknown");
}

TEST_CASE("string to tasklet_event_type", tag)
{
    REQUIRE(
        to_tasklet_event_type("scheduled") == tasklet_event_type::SCHEDULED);
    REQUIRE(to_tasklet_event_type("running") == tasklet_event_type::RUNNING);
    REQUIRE(
        to_tasklet_event_type("before co_await")
        == tasklet_event_type::BEFORE_CO_AWAIT);
    REQUIRE(
        to_tasklet_event_type("after co_await")
        == tasklet_event_type::AFTER_CO_AWAIT);
    REQUIRE(to_tasklet_event_type("finished") == tasklet_event_type::FINISHED);
    REQUIRE(to_tasklet_event_type("unknown") == tasklet_event_type::UNKNOWN);
    REQUIRE(to_tasklet_event_type("other") == tasklet_event_type::UNKNOWN);
}

TEST_CASE("tasklet_info", tag)
{
    tasklet_impl impl{false, "my pool", "my title"};

    tasklet_info info0{impl};
    tasklet_info info1{impl};

    REQUIRE(info0.own_id() == info1.own_id());
    REQUIRE(info0.pool_name() == "my pool");
    REQUIRE(info0.title() == "my title");
    REQUIRE(!info0.have_client());
    auto events = info0.events();
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].what() == tasklet_event_type::SCHEDULED);

    impl.on_finished();
}

TEST_CASE("tasklet_info with client", tag)
{
    tasklet_impl client{false, "client pool", "client title"};
    tasklet_impl impl{false, "my pool", "my title", &client};
    tasklet_info client_info{client};

    tasklet_info info{impl};

    REQUIRE(info.own_id() != client_info.own_id());
    REQUIRE(info.pool_name() == "my pool");
    REQUIRE(info.title() == "my title");
    REQUIRE(info.have_client());
    REQUIRE(info.client_id() == client_info.own_id());
    auto events = info.events();
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].what() == tasklet_event_type::SCHEDULED);

    client.on_finished();
    impl.on_finished();
}

TEST_CASE("get_tasklet_infos", tag)
{
    tasklet_admin admin{true};
    admin.set_capturing_enabled(true);

    create_tasklet_tracker(admin, "my_pool", "title 0");
    auto t1 = create_tasklet_tracker(admin, "my_pool", "title 1");
    create_tasklet_tracker(admin, "my_pool", "title 2");
    t1->on_finished();

    auto most_infos = get_tasklet_infos(admin, false);
    REQUIRE(most_infos.size() == 2);
    REQUIRE(most_infos[0].title() == "title 0");
    REQUIRE(most_infos[1].title() == "title 2");

    auto all_infos = get_tasklet_infos(admin, true);
    REQUIRE(all_infos.size() == 3);
    REQUIRE(all_infos[0].title() == "title 0");
    REQUIRE(all_infos[1].title() == "title 1");
    REQUIRE(all_infos[2].title() == "title 2");
}

TEST_CASE("introspection_set_capturing_enabled", tag)
{
    tasklet_admin admin{true};
    admin.set_capturing_enabled(true);

    introspection_set_capturing_enabled(admin, false);
    create_tasklet_tracker(admin, "my_pool", "title 0");
    REQUIRE(get_tasklet_infos(admin, true).size() == 0);

    introspection_set_capturing_enabled(admin, true);
    create_tasklet_tracker(admin, "my_pool", "title 1");
    REQUIRE(get_tasklet_infos(admin, true).size() == 1);

    introspection_set_capturing_enabled(admin, false);
    create_tasklet_tracker(admin, "my_pool", "title 2");
    REQUIRE(get_tasklet_infos(admin, true).size() == 1);
}

TEST_CASE("introspection_set_logging_enabled", tag)
{
    tasklet_admin admin{true};
    admin.set_capturing_enabled(true);

    introspection_set_logging_enabled(admin, true);
    auto t0 = create_tasklet_tracker(admin, "my_pool", "title 0");
    // Just test that the call succeeds
    t0->log("msg 0");

    introspection_set_logging_enabled(admin, false);
    auto t1 = create_tasklet_tracker(admin, "my_pool", "title 1");
    t1->log("msg 1");
}

TEST_CASE("introspection_clear_info", tag)
{
    tasklet_admin admin{true};
    admin.set_capturing_enabled(true);

    create_tasklet_tracker(admin, "my_pool", "title 0");
    auto t1 = create_tasklet_tracker(admin, "my_pool", "title 1");
    create_tasklet_tracker(admin, "my_pool", "title 2");
    t1->on_finished();
    REQUIRE(get_tasklet_infos(admin, true).size() == 3);

    introspection_clear_info(admin);
    auto all_infos = get_tasklet_infos(admin, true);
    REQUIRE(all_infos.size() == 2);
    REQUIRE(all_infos[0].title() == "title 0");
    REQUIRE(all_infos[1].title() == "title 2");
}
