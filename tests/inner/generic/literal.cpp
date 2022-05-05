#include <cradle/inner/generic/literal.h>

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

TEST_CASE("evaluate literal request", "[generic]")
{
    clean_tasklet_admin_fixture fixture;
    inner_service_core core;
    init_test_inner_service(core);

    literal_request<int> req{87};

    auto res = cppcoro::sync_wait(
        [&]() -> cppcoro::task<int> { co_return co_await req.calculate(); }());

    REQUIRE(res == 87);
}
