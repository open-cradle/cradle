#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../support/concurrency_testing.h"
#include "../support/core.h"
#include "../support/request.h"
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
create_adder_coro(int& num_calls)
{
    return [&](auto& ctx, int a, int b) -> cppcoro::task<int> {
        num_calls += 1;
        co_return a + b;
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

TEST_CASE("evaluate function request - uncached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function<caching_level_type::none>(add, 6, 1)};
    test_resolve_uncached(req, 7, num_add_calls);
}

TEST_CASE("evaluate function request - memory cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req0{rq_function<caching_level_type::memory>(add, 6, 1)};
    auto req1{rq_function<caching_level_type::memory>(add, 5, 3)};

    cached_request_resolution_context ctx;

    // Resolve the two requests, storing the results in the memory cache
    auto res00 = cppcoro::sync_wait(resolve_request(ctx, req0));
    REQUIRE(res00 == 7);
    REQUIRE(num_add_calls == 1);
    auto res10 = cppcoro::sync_wait(resolve_request(ctx, req1));
    REQUIRE(res10 == 8);
    REQUIRE(num_add_calls == 2);

    // Resolve the two requests, retrieving the results from the memory cache
    auto res01 = cppcoro::sync_wait(resolve_request(ctx, req0));
    REQUIRE(res01 == 7);
    REQUIRE(num_add_calls == 2);
    auto res11 = cppcoro::sync_wait(resolve_request(ctx, req1));
    REQUIRE(res11 == 8);
    REQUIRE(num_add_calls == 2);
}

TEST_CASE("evaluate function request (V+V)*S - uncached", "[requests]")
{
    int num_add_calls = 0;
    auto add = create_adder(num_add_calls);
    int num_mul_calls = 0;
    auto mul = create_multiplier(num_mul_calls);
    auto req{rq_function<caching_level_type::none>(
        mul,
        rq_function<caching_level_type::none>(add, 1, 2),
        rq_value_sp(3))};
    test_resolve_uncached(req, 9, num_add_calls, &num_mul_calls);
}

TEST_CASE("evaluate function request (V+V)*S - memory cached", "[requests]")
{
    int num_add_calls = 0;
    auto add = create_adder(num_add_calls);
    int num_mul_calls = 0;
    auto mul = create_multiplier(num_mul_calls);
    auto inner{rq_function<caching_level_type::memory>(add, 1, 2)};
    auto req{
        rq_function<caching_level_type::memory>(mul, inner, rq_value_sp(3))};
    test_resolve_cached(req, 9, num_add_calls, &num_mul_calls);
}

TEST_CASE("evaluate erased function request V+V - uncached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::none>(add, 6, 1)};
    test_resolve_uncached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request V+U - uncached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{
        rq_function_erased<caching_level_type::none>(add, 6, rq_value_up(1))};
    test_resolve_uncached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request V+S - uncached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{
        rq_function_erased<caching_level_type::none>(add, 6, rq_value_sp(1))};
    test_resolve_uncached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request S+V - uncached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{
        rq_function_erased<caching_level_type::none>(add, rq_value_sp(6), 1)};
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
        rq_function_erased<caching_level_type::none>(add, 1, 2),
        rq_value_sp(3))};
    test_resolve_uncached(req, 9, num_add_calls, &num_mul_calls);
}

TEST_CASE("evaluate erased function request V+V - memory cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::memory>(add, 6, 1)};
    test_resolve_cached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request V+U - memory cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::memory>(
        add, 6, rq_value_up(1))};
    test_resolve_cached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request V+S - memory cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::memory>(
        add, 6, rq_value_sp(1))};
    test_resolve_cached(req, 7, num_add_calls);
}

TEST_CASE("evaluate erased function request S+V - memory cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function_erased<caching_level_type::memory>(
        add, rq_value_sp(6), 1)};
    test_resolve_cached(req, 7, num_add_calls);
}

TEST_CASE(
    "evaluate erased function request (V+V)*S - memory cached", "[requests]")
{
    int num_add_calls = 0;
    auto add = create_adder(num_add_calls);
    int num_mul_calls = 0;
    auto mul = create_multiplier(num_mul_calls);
    auto inner{rq_function_erased<caching_level_type::memory>(add, 1, 2)};
    auto req{rq_function_erased<caching_level_type::memory>(
        mul, inner, rq_value_sp(3))};
    test_resolve_cached(req, 9, num_add_calls, &num_mul_calls);
}

TEST_CASE("evaluate erased function request V+V - fully cached", "[requests]")
{
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req_mem{rq_function_erased<caching_level_type::memory>(add, 6, 1)};
    auto req_full{
        rq_function_erased_uuid<caching_level_type::full>("uuid", add, 6, 1)};

    cached_request_resolution_context ctx;
    num_add_calls = 0;

    // Resolving a fully-cached request stores the result in both
    // memory cache and disk cache.
    auto res00 = cppcoro::sync_wait(resolve_request(ctx, req_full));
    sync_wait_write_disk_cache(ctx.get_service());
    REQUIRE(res00 == 7);
    REQUIRE(num_add_calls == 1);

    auto res01 = cppcoro::sync_wait(resolve_request(ctx, req_mem));
    REQUIRE(res01 == 7);
    REQUIRE(num_add_calls == 1);

    auto res02 = cppcoro::sync_wait(resolve_request(ctx, req_full));
    REQUIRE(res02 == 7);
    REQUIRE(num_add_calls == 1);

    // New memory cache
    ctx.reset_memory_cache();
    num_add_calls = 0;

    // Resolving a memory-cached request means a cache miss.
    auto res10 = cppcoro::sync_wait(resolve_request(ctx, req_mem));
    REQUIRE(res10 == 7);
    REQUIRE(num_add_calls == 1);

    // New memory cache, same disk cache
    ctx.reset_memory_cache();
    num_add_calls = 0;

    // Resolving a fully-cached request means a disk cache hit,
    // and the result is stored in the memory cache as well.
    auto res20 = cppcoro::sync_wait(resolve_request(ctx, req_full));
    REQUIRE(res20 == 7);
    REQUIRE(num_add_calls == 0);

    // So now resolving a memory-cached request finds the result in
    // the memory cache.
    auto res21 = cppcoro::sync_wait(resolve_request(ctx, req_mem));
    REQUIRE(res21 == 7);
    REQUIRE(num_add_calls == 0);
}

TEST_CASE(
    "evaluate function requests in parallel - uncached function", "[requests]")
{
    static constexpr int num_requests = 7;
    using Req
        = function_request_erased<caching_level_type::none, int, false, false>;
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    uncached_request_resolution_context ctx{};
    std::vector<Req> requests;
    for (int i = 0; i < num_requests; ++i)
    {
        requests.emplace_back(
            rq_function_erased<caching_level_type::none>(add, i, i * 2));
    }

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res[i] == i * 3);
    }
    REQUIRE(num_add_calls == num_requests);
}

TEST_CASE(
    "evaluate function requests in parallel - uncached coroutine",
    "[requests]")
{
    static constexpr int num_requests = 7;
    using Value = int;
    using Req = function_request_erased<
        caching_level_type::none,
        Value,
        false,
        true>;
    int num_add_calls{};
    auto add{create_adder_coro(num_add_calls)};
    uncached_request_resolution_context ctx{};
    std::vector<Req> requests;
    for (int i = 0; i < num_requests; ++i)
    {
        requests.emplace_back(
            rq_function_erased_coro<caching_level_type::none, Value>(
                add, i, i * 2));
    }

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res[i] == i * 3);
    }
    REQUIRE(num_add_calls == num_requests);
}

TEST_CASE(
    "evaluate function requests in parallel - memory cached", "[requests]")
{
    static constexpr int num_requests = 7;
    using Req = function_request_erased<
        caching_level_type::memory,
        int,
        false,
        false>;
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    cached_request_resolution_context ctx{};
    std::vector<Req> requests;
    for (int i = 0; i < num_requests; ++i)
    {
        requests.emplace_back(
            rq_function_erased<caching_level_type::memory>(add, i, i * 2));
    }

    auto res0 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res0.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res0[i] == i * 3);
    }
    REQUIRE(num_add_calls == num_requests);

    auto res1 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res1.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res1[i] == i * 3);
    }
    REQUIRE(num_add_calls == num_requests);
}

TEST_CASE("evaluate function requests in parallel - disk cached", "[requests]")
{
    static constexpr int num_requests = 7;
    using Req
        = function_request_erased<caching_level_type::full, int, false, false>;
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    cached_request_resolution_context ctx{};
    std::vector<Req> requests;
    for (int i = 0; i < num_requests; ++i)
    {
        std::ostringstream os;
        os << "uuid " << i;
        requests.emplace_back(
            rq_function_erased_uuid<caching_level_type::full>(
                os.str(), add, i, i * 2));
    }

    auto res0 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
    sync_wait_write_disk_cache(ctx.get_service());

    REQUIRE(res0.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res0[i] == i * 3);
    }
    REQUIRE(num_add_calls == num_requests);
    auto ic0 = get_summary_info(ctx.get_cache());
    REQUIRE(ic0.entry_count == num_requests);
    auto dc0
        = ctx.get_service().inner_internals().disk_cache.get_summary_info();
    REQUIRE(dc0.entry_count == num_requests);

    ctx.reset_memory_cache();
    REQUIRE(get_summary_info(ctx.get_cache()).entry_count == 0);
    auto res1 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res1.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res1[i] == i * 3);
    }
    REQUIRE(num_add_calls == num_requests);
    auto ic1 = get_summary_info(ctx.get_cache());
    REQUIRE(ic1.entry_count == num_requests);
    auto dc1
        = ctx.get_service().inner_internals().disk_cache.get_summary_info();
    REQUIRE(dc1.entry_count == num_requests);
}
