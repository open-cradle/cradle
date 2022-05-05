#include <cradle/inner/generic/addition.h>
#include <cradle/inner/generic/literal.h>

#include <catch2/catch.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../introspection/tasklet_testing.h"
#include "../support/core.h"
#include <cradle/inner/service/core.h>

using namespace cradle;

CEREAL_REGISTER_TYPE(cradle::literal_request<int>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(
    cradle::abstract_request<int>, cradle::literal_request<int>)

TEST_CASE("serialize addition request", "[generic]")
{
    using Value = int;
    using AdditionRequest = addition_request<Value>;
    using LiteralRequest = literal_request<Value>;
    std::stringstream ss;

    {
        std::vector<std::shared_ptr<abstract_request<Value>>> subrequests;
        for (int i = 0; i < 4; ++i)
        {
            subrequests.emplace_back(std::make_shared<LiteralRequest>(i + 1));
        }
        AdditionRequest req{subrequests};
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(req);
    }

    {
        AdditionRequest req1;
        cereal::BinaryInputArchive iarchive(ss);
        iarchive(req1);

        REQUIRE(req1.get_summary() == "addition");
        for (int i = 0; i < 4; ++i)
        {
            auto subreq = req1.get_subrequests()[i];
            LiteralRequest* litreq = dynamic_cast<LiteralRequest*>(&*subreq);
            REQUIRE(litreq != nullptr);
            REQUIRE(litreq->get_literal() == i + 1);
        }
    }
}

TEST_CASE("evaluate addition request", "[generic]")
{
    clean_tasklet_admin_fixture fixture;
    inner_service_core core;
    init_test_inner_service(core);

    using Value = int;
    using LiteralRequest = literal_request<Value>;
    std::stringstream ss;

    std::vector<std::shared_ptr<abstract_request<Value>>> subrequests;
    for (int i = 0; i < 4; ++i)
    {
        subrequests.emplace_back(std::make_shared<LiteralRequest>(i + 1));
    }
    auto client = create_tasklet_tracker("client_pool", "client_title");
    auto shared_req{make_shared_addition_request(core, client, subrequests)};

    auto res = cppcoro::sync_wait([&]() -> cppcoro::task<int> {
        co_return co_await shared_req->calculate();
    }());

    REQUIRE(res == 10);
    auto events = latest_tasklet_info().events();
    REQUIRE(events.size() == 3);
    REQUIRE(events[0].what() == tasklet_event_type::SCHEDULED);
    REQUIRE(events[1].what() == tasklet_event_type::BEFORE_CO_AWAIT);
    REQUIRE(events[1].details().starts_with("addition "));
    REQUIRE(events[2].what() == tasklet_event_type::AFTER_CO_AWAIT);
}
