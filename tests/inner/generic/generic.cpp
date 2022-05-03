#include <cradle/inner/generic/addition.h>
#include <cradle/inner/generic/generic.h>

#include <fstream>
#include <sstream>

#include <catch2/catch.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../introspection/tasklet_testing.h"
#include "../support/core.h"
#include <cradle/inner/service/core.h>

using namespace cradle;

TEST_CASE("serialize literal request", "[generic]")
{
    using Value = int;

    std::stringstream ss;

    {
        literal_request<Value> req{87};
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(req);
    }

    {
        literal_request<Value> req1;
        cereal::BinaryInputArchive iarchive(ss);
        iarchive(req1);

        REQUIRE(req1.get_literal() == 87);
    }
}

TEST_CASE("make shared task for literal request", "[generic]")
{
    clean_tasklet_admin_fixture fixture;
    inner_service_core core;
    init_test_inner_service(core);

    using Request = literal_request<int>;
    auto req{std::make_shared<Request>(87)};
    auto me = make_shared_task_for_request(core, req, nullptr);

    auto res = cppcoro::sync_wait(
        [&]() -> cppcoro::task<int> { co_return co_await me; }());

    std::cout << "res " << res << "\n";
    REQUIRE(res == 87);
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

        std::cout << "req1.summary " << req1.get_summary() << "\n";
        REQUIRE(req1.get_subrequests()[0]->get_literal() == 1);
        REQUIRE(req1.get_subrequests()[1]->get_literal() == 2);
        REQUIRE(req1.get_subrequests()[2]->get_literal() == 3);
        REQUIRE(req1.get_subrequests()[3]->get_literal() == 4);
        // std::cout << "req.addition " << req.get_addition() << "\n";
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
    auto me = make_shared_task_for_request(core, req, nullptr);

    auto res = cppcoro::sync_wait(
        [&]() -> cppcoro::task<int> { co_return co_await me; }());

    std::cout << "res " << res << "\n";
    REQUIRE(res == 10);
}
