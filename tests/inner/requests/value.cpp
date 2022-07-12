#include <cradle/inner/requests/value.h>

#include <fstream>
#include <sstream>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../introspection/tasklet_testing.h"
#include "../support/core.h"
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/core.h>
#include <cradle/inner/service/request.h>

using namespace cradle;

TEST_CASE("create value request", "[requests]")
{
    std::string s0{"abc"};
    auto req0{rq_value(s0)};
    REQUIRE(req0.get_value() == std::string{"abc"});

    auto req1{rq_value(std::string{"def"})};
    REQUIRE(req1.get_value() == std::string{"def"});
}

TEST_CASE("evaluate value request", "[requests]")
{
    uncached_request_resolution_context ctx{};

    auto req{rq_value(87)};

    auto res = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res == 87);
}
