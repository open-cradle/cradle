#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../support/core.h"
#include <cradle/inner/service/core.h>
#include <cradle/inner/service/request.h>

using namespace cradle;

auto
create_adder(int& num_calls)
{
    return [&](int a, int b) {
        num_calls += 1;
        return a + b;
    };
}

auto
create_multiplier(int& num_calls)
{
    return [&](int a, int b) {
        num_calls += 1;
        return a * b;
    };
}

template<typename Request>
void
test_resolve_uncached(
    Request const& req,
    int expected,
    int& num_calls1,
    int* num_calls2 = nullptr)
{
    uncached_request_resolution_context ctx;

    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res0 == expected);
    REQUIRE(num_calls1 == 1);
    if (num_calls2)
    {
        REQUIRE(*num_calls2 == 1);
    }

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res1 == expected);
    REQUIRE(num_calls1 == 2);
    if (num_calls2)
    {
        REQUIRE(*num_calls2 == 2);
    }
}

template<typename Request>
void
test_resolve_cached(
    Request const& req,
    int expected,
    int& num_calls1,
    int* num_calls2 = nullptr)
{
    cached_request_resolution_context ctx;

    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res0 == expected);
    REQUIRE(num_calls1 == 1);
    if (num_calls2)
    {
        REQUIRE(*num_calls2 == 1);
    }

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res1 == expected);
    REQUIRE(num_calls1 == 1);
    if (num_calls2)
    {
        REQUIRE(*num_calls2 == 1);
    }
}

#if 0
TEST_CASE("evaluate function request - uncached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{
        rq_function<caching_level_type::none>(add, rq_value(6), rq_value(1))};
    test_resolve_uncached(req, 7, num_add_calls);
}

TEST_CASE("evaluate function request - memory cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function<caching_level_type::memory>(
        add, rq_value(6), rq_value(1))};
    test_resolve_cached(req, 7, num_add_calls);
}
#endif

TEST_CASE("evaluate erased function request V+V - uncached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::none>(
        add, rq_value(6), rq_value(1))};
    test_resolve_uncached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request V+U - uncached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::none>(
        add, rq_value(6), rq_value_up(1))};
    test_resolve_uncached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request V+S - uncached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::none>(
        add, rq_value(6), rq_value_sp(1))};
    test_resolve_uncached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request S+V - uncached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::none>(
        add, rq_value_sp(6), rq_value(1))};
    test_resolve_uncached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request (V+V)*S - uncached", "[requests]")
{
    int num_add_calls = 0;
    auto add = create_adder(num_add_calls);
    int num_mul_calls = 0;
    auto mul = create_multiplier(num_mul_calls);
    auto req{rq_function_erased<caching_level_type::none>(
        mul,
        rq_function_erased<caching_level_type::none>(
            add, rq_value(1), rq_value(2)),
        rq_value_sp(3))};
    test_resolve_uncached(req, 9, num_add_calls, &num_mul_calls);
}

TEST_CASE("evaluate erased function request V+V - memory cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::memory>(
        add, rq_value(6), rq_value(1))};
    test_resolve_cached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request V+U - memory cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::memory>(
        add, rq_value(6), rq_value_up(1))};
    test_resolve_cached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request V+S - memory cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::memory>(
        add, rq_value(6), rq_value_sp(1))};
    test_resolve_cached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request S+V - memory cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::memory>(
        add, rq_value_sp(6), rq_value(1))};
    test_resolve_cached(req, 7, num_add_calls);
}

TEST_CASE(
    "evaluate erased function request (V+V)*S - memory cached", "[requests]")
{
    int num_add_calls = 0;
    auto add = create_adder(num_add_calls);
    int num_mul_calls = 0;
    auto mul = create_multiplier(num_mul_calls);
    auto inner{rq_function_erased<caching_level_type::memory>(
        add, rq_value(1), rq_value(2))};
    auto req{rq_function_erased<caching_level_type::memory>(
        mul, inner, rq_value_sp(3))};
    test_resolve_cached(req, 9, num_add_calls, &num_mul_calls);
}

TEST_CASE("evaluate erased function request V+V - fully cached", "[nu]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::full>(
        add, rq_value(6), rq_value(1))};
    test_resolve_cached(req, 7, num_add_calls);
}
