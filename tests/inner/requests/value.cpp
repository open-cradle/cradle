#include <cradle/inner/requests/value.h>

#include <fstream>
#include <sstream>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../support/inner_service.h"
#include "../../support/request.h"
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/service/resources.h>

using namespace cradle;

static char const tag[] = "[inner][requests][value]";

TEST_CASE("create value request", tag)
{
    std::string s0{"abc"};
    auto req0{rq_value(s0)};
    REQUIRE(req0.get_value() == std::string{"abc"});

    auto req1{rq_value(std::string{"def"})};
    REQUIRE(req1.get_value() == std::string{"def"});
}

TEST_CASE("evaluate value request", tag)
{
    auto resources{make_inner_test_resources()};
    non_caching_request_resolution_context ctx{*resources};

    auto req{rq_value(87)};

    auto res = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res == 87);
}

TEST_CASE("evaluate value requests in parallel", tag)
{
    static constexpr int num_requests = 7;
    auto resources{make_inner_test_resources()};
    non_caching_request_resolution_context ctx{*resources};
    std::vector<value_request<int>> requests;
    for (int i = 0; i < num_requests; ++i)
    {
        requests.emplace_back(rq_value(i * 3));
    }

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res[i] == i * 3);
    }
}
