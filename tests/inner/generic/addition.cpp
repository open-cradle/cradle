#include <cradle/inner/generic/addition.h>

#include <catch2/catch.hpp>
#include <cereal/archives/binary.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../introspection/tasklet_testing.h"
#include "../support/core.h"
#include <cradle/inner/service/core.h>

using namespace cradle;

// A newly created tasklet will be the latest one for which
// info can be retrieved.
static tasklet_info
latest_tasklet_info()
{
    auto all_infos = get_tasklet_infos(true);
    REQUIRE(all_infos.size() > 0);
    return all_infos[all_infos.size() - 1];
}

TEST_CASE("serialize addition request", "[generic]")
{
    using Value = int;
    using Subtype = literal_request<Value>;
    std::stringstream ss;

    {
        std::vector<std::shared_ptr<Subtype>> subrequests{
            std::make_shared<Subtype>(1),
            std::make_shared<Subtype>(2),
            std::make_shared<Subtype>(3),
            std::make_shared<Subtype>(4),
        };
        addition_request req{subrequests};
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(req);
    }

    {
        addition_request req1;
        cereal::BinaryInputArchive iarchive(ss);
        iarchive(req1);

        REQUIRE(req1.get_summary() == "addition");
        REQUIRE(req1.get_subrequests()[0]->get_literal() == 1);
        REQUIRE(req1.get_subrequests()[1]->get_literal() == 2);
        REQUIRE(req1.get_subrequests()[2]->get_literal() == 3);
        REQUIRE(req1.get_subrequests()[3]->get_literal() == 4);
    }
}

TEST_CASE("make shared task for addition request", "[generic]")
{
    clean_tasklet_admin_fixture fixture;
    inner_service_core core;
    init_test_inner_service(core);

    using Request = addition_request;
    using Value = int;
    using Subtype = literal_request<Value>;
    std::vector<std::shared_ptr<Subtype>> subrequests{
        std::make_shared<Subtype>(1),
        std::make_shared<Subtype>(2),
        std::make_shared<Subtype>(3),
        std::make_shared<Subtype>(4),
    };

    auto req{std::make_shared<Request>(subrequests)};
    auto client = create_tasklet_tracker("client_pool", "client_title");
    auto me = make_shared_task_for_request(core, req, client);

    auto res = cppcoro::sync_wait(
        [&]() -> cppcoro::task<int> { co_return co_await me; }());

    REQUIRE(res == 10);
    auto events = latest_tasklet_info().events();
    REQUIRE(events.size() == 3);
    REQUIRE(events[0].what() == tasklet_event_type::SCHEDULED);
    REQUIRE(events[1].what() == tasklet_event_type::BEFORE_CO_AWAIT);
    REQUIRE(events[1].details().starts_with("addition "));
    REQUIRE(events[2].what() == tasklet_event_type::AFTER_CO_AWAIT);
}
