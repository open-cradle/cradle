#include <cradle/inner/generic/generic.h>

#include <fstream>

#include <catch2/catch.hpp>
#include <cereal/archives/json.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../introspection/tasklet_testing.h"
#include "../support/core.h"
#include <cradle/inner/service/core.h>

using namespace cradle;

TEST_CASE("test", "[generic]")
{
    using Value = int;

    {
        literal_request<Value> req{87};
        std::ofstream os{"bla.json"};
        cereal::JSONOutputArchive ar(os);
        ar(CEREAL_NVP(req));
    }

    {
        // Variable name must match JSON contents?!
        literal_request<Value> req;
        std::ifstream is{"bla.json"};
        cereal::JSONInputArchive ar(is);
        ar(CEREAL_NVP(req));

        // std::cout << "req.summary " << req.get_summary() << "\n";
        std::cout << "req.literal " << req.get_literal() << "\n";
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
