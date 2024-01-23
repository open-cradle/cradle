#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>

#include "../../support/concurrency_testing.h"
#include "../../support/inner_service.h"
#include "../../support/request.h"
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][resolve][request]";

request_uuid
make_test_uuid(int ext)
{
    return request_uuid{fmt::format("{}-{:04d}", tag, ext)};
}

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
    inner_resources& resources,
    int expected,
    int& num_calls1,
    int* num_calls2 = nullptr)
{
    non_caching_request_resolution_context ctx{resources};

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
    inner_resources& resources,
    int expected,
    int& num_calls1,
    int* num_calls2 = nullptr)
{
    caching_request_resolution_context ctx{resources};

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

} // namespace

TEST_CASE("evaluate function request V+V - uncached", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::none> props{make_test_uuid(0)};
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function(props, add, 6, 1)};
    test_resolve_uncached(req, *resources, 7, num_add_calls);
}

TEST_CASE("evaluate function request V+V - memory cached", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::memory> props{make_test_uuid(10)};
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function(props, add, 6, 1)};
    test_resolve_cached(req, *resources, 7, num_add_calls);
}

TEST_CASE("evaluate dual function request V+V - memory cached", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::memory> props0{make_test_uuid(20)};
    request_props<caching_level_type::memory> props1{make_test_uuid(21)};
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req0{rq_function(props0, add, 6, 1)};
    auto req1{rq_function(props1, add, 5, 3)};

    caching_request_resolution_context ctx{*resources};

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

TEST_CASE("evaluate function request (V+V)*V - uncached", tag)
{
    auto resources{make_inner_test_resources()};
    using Props = request_props<caching_level_type::none>;
    Props props_mul{make_test_uuid(40)};
    Props props_add{make_test_uuid(41)};
    int num_add_calls = 0;
    auto add = create_adder(num_add_calls);
    int num_mul_calls = 0;
    auto mul = create_multiplier(num_mul_calls);
    auto req{
        rq_function(props_mul, mul, rq_function(props_add, add, 1, 2), 3)};
    test_resolve_uncached(req, *resources, 9, num_add_calls, &num_mul_calls);
}

TEST_CASE("evaluate function request (V+V)*V - memory cached", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::memory> props_inner{make_test_uuid(90)};
    request_props<caching_level_type::memory> props_main{make_test_uuid(91)};
    int num_add_calls = 0;
    auto add = create_adder(num_add_calls);
    int num_mul_calls = 0;
    auto mul = create_multiplier(num_mul_calls);
    auto inner{rq_function(props_inner, add, 1, 2)};
    auto req{rq_function(props_main, mul, inner, 3)};
    test_resolve_cached(req, *resources, 9, num_add_calls, &num_mul_calls);
}

TEST_CASE("evaluate function request V+V - fully cached", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::full> props_full{make_test_uuid(201)};
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req_full{rq_function(props_full, add, 6, 1)};

    caching_request_resolution_context ctx{*resources};
    num_add_calls = 0;

    // Resolving a fully-cached request stores the result in both
    // memory cache and disk cache.
    auto res00 = cppcoro::sync_wait(resolve_request(ctx, req_full));
    sync_wait_write_disk_cache(*resources);
    REQUIRE(res00 == 7);
    REQUIRE(num_add_calls == 1);

    // Resolving the same request again, the result comes from a cache
    // (the memory cache, although we cannot see that).
    auto res02 = cppcoro::sync_wait(resolve_request(ctx, req_full));
    REQUIRE(res02 == 7);
    REQUIRE(num_add_calls == 1);

    // New memory cache, same disk cache
    ctx.reset_memory_cache();

    // The result still comes from a cache; this time, we know it must be the
    // disk cache.
    auto res20 = cppcoro::sync_wait(resolve_request(ctx, req_full));
    REQUIRE(res20 == 7);
    REQUIRE(num_add_calls == 1);
}

TEST_CASE("evaluate function requests in parallel - uncached function", tag)
{
    auto resources{make_inner_test_resources()};
    static constexpr int num_requests = 7;
    using Value = int;
    using Props = request_props<caching_level_type::none>;
    using Req = function_request<Value, Props>;
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    non_caching_request_resolution_context ctx{*resources};
    std::vector<Req> requests;
    for (int i = 0; i < num_requests; ++i)
    {
        Props props{make_test_uuid(100 + i)};
        requests.emplace_back(rq_function(props, add, i, i * 2));
    }

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res[i] == i * 3);
    }
    REQUIRE(num_add_calls == num_requests);
}

TEST_CASE("evaluate function requests in parallel - uncached coroutine", tag)
{
    auto resources{make_inner_test_resources()};
    static constexpr int num_requests = 7;
    using Value = int;
    using Props = request_props<
        caching_level_type::none,
        request_function_t::coro,
        false>;
    using Req = function_request<Value, Props>;
    int num_add_calls{};
    auto add{create_adder_coro(num_add_calls)};
    non_caching_request_resolution_context ctx{*resources};
    std::vector<Req> requests;
    for (int i = 0; i < num_requests; ++i)
    {
        Props props{make_test_uuid(300 + i)};
        requests.emplace_back(rq_function(props, add, i, i * 2));
    }

    auto res = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res[i] == i * 3);
    }
    REQUIRE(num_add_calls == num_requests);
}

TEST_CASE("evaluate function requests in parallel - memory cached", tag)
{
    auto resources{make_inner_test_resources()};
    static constexpr int num_requests = 7;
    using Value = int;
    using Props = request_props<caching_level_type::memory>;
    using Req = function_request<Value, Props>;
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    caching_request_resolution_context ctx{*resources};
    std::vector<Req> requests;
    for (int i = 0; i < num_requests; ++i)
    {
        Props props{make_test_uuid(400 + i)};
        requests.emplace_back(rq_function(props, add, i, i * 2));
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

TEST_CASE("evaluate function requests in parallel - disk cached", tag)
{
    auto resources{make_inner_test_resources()};
    static constexpr int num_requests = 7;
    using Value = int;
    using Props = request_props<caching_level_type::full>;
    using Req = function_request<Value, Props>;
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    caching_request_resolution_context ctx{*resources};
    auto& disk_cache{
        static_cast<local_disk_cache&>(resources->secondary_cache())};
    std::vector<Req> requests;
    for (int i = 0; i < num_requests; ++i)
    {
        std::ostringstream os;
        os << "uuid " << i;
        requests.emplace_back(
            rq_function(Props(request_uuid(os.str())), add, i, i * 2));
    }

    auto res0 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));
    sync_wait_write_disk_cache(*resources);

    auto& mem_cache{resources->memory_cache()};
    REQUIRE(res0.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res0[i] == i * 3);
    }
    REQUIRE(num_add_calls == num_requests);
    auto ic0 = get_summary_info(mem_cache);
    REQUIRE(ic0.ac_num_records == num_requests);
    auto dc0 = disk_cache.get_summary_info();
    REQUIRE(dc0.ac_entry_count == num_requests);

    ctx.reset_memory_cache();
    REQUIRE(get_summary_info(mem_cache).ac_num_records == 0);
    auto res1 = cppcoro::sync_wait(resolve_in_parallel(ctx, requests));

    REQUIRE(res1.size() == num_requests);
    for (int i = 0; i < num_requests; ++i)
    {
        REQUIRE(res1[i] == i * 3);
    }
    REQUIRE(num_add_calls == num_requests);
    auto ic1 = get_summary_info(mem_cache);
    REQUIRE(ic1.ac_num_records == num_requests);
    auto dc1 = disk_cache.get_summary_info();
    REQUIRE(dc1.ac_entry_count == num_requests);
}

static auto add2 = [](int a, int b) { return a + b; };

TEST_CASE("resolve function_request with subrequest", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::memory> props0{make_test_uuid(500)};
    request_props<caching_level_type::memory> props1{make_test_uuid(501)};
    request_props<caching_level_type::memory> props2{make_test_uuid(502)};
    auto req0{rq_function(props0, add2, 1, 2)};
    auto req1{rq_function(props1, add2, req0, 3)};
    auto req2{rq_function(props2, add2, req1, 4)};
    caching_request_resolution_context ctx{*resources};

    REQUIRE(cppcoro::sync_wait(resolve_request(ctx, req0)) == 3);
    REQUIRE(cppcoro::sync_wait(resolve_request(ctx, req1)) == 6);
    // The following shouldn't assert even if function_request_impl::hash()
    // is modified to always return the same value.
    REQUIRE(cppcoro::sync_wait(resolve_request(ctx, req2)) == 10);
}

TEST_CASE("evaluate function request - memory cache behavior", tag)
{
    auto resources{make_inner_test_resources()};
    auto& mem_cache{resources->memory_cache()};
    request_props<caching_level_type::memory> props{make_test_uuid(600)};
    int num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function(props, add, 6, 3)};

    // Initially, the memory cache should be empty.
    auto info0{get_summary_info(mem_cache)};
    CHECK(info0.ac_num_records_in_use == 0);
    CHECK(info0.ac_num_records_pending_eviction == 0);
    CHECK(info0.cas_num_records == 0);

    // Creating the task should create a pointer to a new cache record.
    caching_request_resolution_context ctx{*resources};
    auto task = resolve_request(ctx, req);
    auto info1{get_summary_info(mem_cache)};
    CHECK(info1.ac_num_records_in_use == 1);
    CHECK(info1.ac_num_records_pending_eviction == 0);
    CHECK(info1.cas_num_records == 0);

    // Resolving the request (running the task) should create an entry in the
    // CAS. The task should still hold a reference to the record-in-use.
    auto res0 = cppcoro::sync_wait(task);
    REQUIRE(res0 == 9);
    auto info2{get_summary_info(mem_cache)};
    CHECK(info2.ac_num_records_in_use == 1);
    CHECK(info2.ac_num_records_pending_eviction == 0);
    CHECK(info2.cas_num_records == 1);

    // The task owns the ptr which is the only reference to the record-in-use.
    // After deleting the task, the record is no longer in use.
    task = decltype(task)();
    auto info3{get_summary_info(mem_cache)};
    CHECK(info3.ac_num_records_in_use == 0);
    CHECK(info3.ac_num_records_pending_eviction == 1);
    CHECK(info3.cas_num_records == 1);
}
