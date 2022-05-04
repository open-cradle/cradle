#include <cradle/inner/generic/addition.h>
#include <cradle/inner/generic/generic.h>

#include <fstream>
#include <sstream>

#include <catch2/catch.hpp>
#include <cereal/archives/binary.hpp>
#include <cradle/inner/generic/generic.h>

#include <catch2/catch.hpp>
#include <cereal/archives/binary.hpp>
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

    REQUIRE(res == 87);
}
