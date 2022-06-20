#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../support/core.h"
#include <cradle/inner/service/core.h>
#include <cradle/inner/service/request.h>

using namespace cradle;

TEST_CASE("evaluate function request - uncached", "[requests]")
{
    int num_add_calls = 0;
    auto add = [&](int a, int b) {
        num_add_calls += 1;
        return a + b;
    };
    auto req{
        rq_function<caching_level_type::none>(add, rq_value(6), rq_value(1))};
    uncached_request_resolution_context ctx;

    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res0 == 7);
    REQUIRE(num_add_calls == 1);

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res1 == 7);
    REQUIRE(num_add_calls == 2);
}

TEST_CASE("evaluate function request - memory cached", "[requests]")
{
    int num_add_calls = 0;
    auto add = [&](int a, int b) {
        num_add_calls += 1;
        return a + b;
    };
    auto req{rq_function<caching_level_type::memory>(
        add, rq_value(6), rq_value(1))};
    memory_cached_request_resolution_context ctx;

    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res0 == 7);
    REQUIRE(num_add_calls == 1);

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res1 == 7);
    REQUIRE(num_add_calls == 1);
}

TEST_CASE("evaluate function request - erased, uncached", "[requests]")
{
    int num_add_calls = 0;
    auto add = [&](int a, int b) {
        num_add_calls += 1;
        return a + b;
    };
    auto req{rq_function_erased<caching_level_type::none>(
        add, rq_value(6), rq_value(1))};
    uncached_request_resolution_context ctx;

    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res0 == 7);
    REQUIRE(num_add_calls == 1);

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res1 == 7);
    REQUIRE(num_add_calls == 2);
}

TEST_CASE("evaluate function request - erased, memory cached", "[requests]")
{
    int num_add_calls = 0;
    auto add = [&](int a, int b) {
        num_add_calls += 1;
        return a + b;
    };
    auto req{rq_function_erased<caching_level_type::memory>(
        add, rq_value(6), rq_value(1))};
    memory_cached_request_resolution_context ctx;

    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res0 == 7);
    REQUIRE(num_add_calls == 1);

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res1 == 7);
    REQUIRE(num_add_calls == 1);
}