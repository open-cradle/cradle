#include <catch2/catch.hpp>

#include "../../support/inner_service.h"
#include "../../support/tasklet_testing.h"
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_impl.h>
#include <cradle/inner/introspection/tasklet_info.h>

using namespace cradle;

TEST_CASE("tasklet_event", "[introspection]")
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

TEST_CASE("tasklet_event with details", "[introspection]")
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

TEST_CASE("tasklet_info", "[introspection]")
{
    tasklet_impl impl{"my pool", "my title"};

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

TEST_CASE("tasklet_info with client", "[introspection]")
{
    tasklet_impl client{"client pool", "client title"};
    tasklet_impl impl{"my pool", "my title", &client};
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

TEST_CASE("get_tasklet_infos", "[introspection]")
{
    clean_tasklet_admin_fixture fixture;

    create_tasklet_tracker("my_pool", "title 0");
    auto t1 = create_tasklet_tracker("my_pool", "title 1");
    create_tasklet_tracker("my_pool", "title 2");
    t1->on_finished();

    auto most_infos = get_tasklet_infos(false);
    REQUIRE(most_infos.size() == 2);
    REQUIRE(most_infos[0].title() == "title 0");
    REQUIRE(most_infos[1].title() == "title 2");

    auto all_infos = get_tasklet_infos(true);
    REQUIRE(all_infos.size() == 3);
    REQUIRE(all_infos[0].title() == "title 0");
    REQUIRE(all_infos[1].title() == "title 1");
    REQUIRE(all_infos[2].title() == "title 2");
}

TEST_CASE("introspection_set_capturing_enabled", "[introspection]")
{
    clean_tasklet_admin_fixture fixture;

    introspection_set_capturing_enabled(false);
    create_tasklet_tracker("my_pool", "title 0");
    REQUIRE(get_tasklet_infos(true).size() == 0);

    introspection_set_capturing_enabled(true);
    create_tasklet_tracker("my_pool", "title 1");
    REQUIRE(get_tasklet_infos(true).size() == 1);

    introspection_set_capturing_enabled(false);
    create_tasklet_tracker("my_pool", "title 2");
    REQUIRE(get_tasklet_infos(true).size() == 1);
}

TEST_CASE("introspection_set_logging_enabled", "[introspection]")
{
    clean_tasklet_admin_fixture fixture;

    introspection_set_logging_enabled(true);
    auto t0 = create_tasklet_tracker("my_pool", "title 0");
    // Just test that the call succeeds
    t0->log("msg 0");

    introspection_set_logging_enabled(false);
    auto t1 = create_tasklet_tracker("my_pool", "title 1");
    t1->log("msg 1");
}

TEST_CASE("introspection_clear_info", "[introspection]")
{
    clean_tasklet_admin_fixture fixture;

    create_tasklet_tracker("my_pool", "title 0");
    auto t1 = create_tasklet_tracker("my_pool", "title 1");
    create_tasklet_tracker("my_pool", "title 2");
    t1->on_finished();
    REQUIRE(get_tasklet_infos(true).size() == 3);

    introspection_clear_info();
    auto all_infos = get_tasklet_infos(true);
    REQUIRE(all_infos.size() == 2);
    REQUIRE(all_infos[0].title() == "title 0");
    REQUIRE(all_infos[1].title() == "title 2");
}
