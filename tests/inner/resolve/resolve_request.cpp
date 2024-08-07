#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>

#include "../../support/concurrency_testing.h"
#include "../../support/inner_service.h"
#include "../../support/make_test_blob.h"
#include "../../support/request.h"
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/demo_class.h>
#include <cradle/plugins/domain/testing/demo_class_requests.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/secondary_cache/simple/simple_storage.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][resolve][request]";

request_uuid
make_test_uuid(int ext)
{
    return request_uuid{fmt::format("{}-{:04d}", tag, ext)};
}

auto
create_adder(std::atomic<int>& num_calls)
{
    return [&](int a, int b) {
        num_calls += 1;
        return a + b;
    };
}

auto
create_adder_coro(std::atomic<int>& num_calls)
{
    return [&](auto& ctx, int a, int b) -> cppcoro::task<int> {
        num_calls += 1;
        co_return a + b;
    };
}

auto
create_multiplier(std::atomic<int>& num_calls)
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
    std::atomic<int>& num_calls1,
    std::atomic<int>* num_calls2 = nullptr)
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
    std::atomic<int>& num_calls1,
    std::atomic<int>* num_calls2 = nullptr)
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
    std::atomic<int> num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function(props, add, 6, 1)};
    test_resolve_uncached(req, *resources, 7, num_add_calls);
}

TEST_CASE("evaluate function request V+V - memory cached", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::memory> props{make_test_uuid(10)};
    std::atomic<int> num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function(props, add, 6, 1)};
    test_resolve_cached(req, *resources, 7, num_add_calls);
}

TEST_CASE("evaluate dual function request V+V - memory cached", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::memory> props0{make_test_uuid(20)};
    request_props<caching_level_type::memory> props1{make_test_uuid(21)};
    std::atomic<int> num_add_calls{};
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
    std::atomic<int> num_add_calls = 0;
    auto add = create_adder(num_add_calls);
    std::atomic<int> num_mul_calls = 0;
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
    std::atomic<int> num_add_calls = 0;
    auto add = create_adder(num_add_calls);
    std::atomic<int> num_mul_calls = 0;
    auto mul = create_multiplier(num_mul_calls);
    auto inner{rq_function(props_inner, add, 1, 2)};
    auto req{rq_function(props_main, mul, inner, 3)};
    test_resolve_cached(req, *resources, 9, num_add_calls, &num_mul_calls);
}

TEST_CASE("evaluate function request V+V - fully cached", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::full> props_full{make_test_uuid(201)};
    std::atomic<int> num_add_calls{};
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
    resources->reset_memory_cache();

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
    using ObjectProps = make_request_object_props_type<Props>;
    using Req = function_request<Value, ObjectProps>;
    std::atomic<int> num_add_calls{};
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
    using ObjectProps = make_request_object_props_type<Props>;
    using Req = function_request<Value, ObjectProps>;
    std::atomic<int> num_add_calls{};
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
    using ObjectProps = make_request_object_props_type<Props>;
    using Req = function_request<Value, ObjectProps>;
    std::atomic<int> num_add_calls{};
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
    using ObjectProps = make_request_object_props_type<Props>;
    using Req = function_request<Value, ObjectProps>;
    std::atomic<int> num_add_calls{};
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

    resources->reset_memory_cache();
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
    std::atomic<int> num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function(props, add, 6, 3)};

    // Initially, the memory cache should be empty.
    auto info0{get_summary_info(mem_cache)};
    CHECK(info0.ac_num_records_in_use == 0);
    CHECK(info0.ac_num_records_pending_eviction == 0);
    CHECK(info0.cas_num_records == 0);

    // In the current implementation, creating the task does not yet create a
    // pointer to a new cache record.
    caching_request_resolution_context ctx{*resources};
    auto task = resolve_request(ctx, req);
    auto info1{get_summary_info(mem_cache)};
    CHECK(info1.ac_num_records_in_use == 0);
    CHECK(info1.ac_num_records_pending_eviction == 0);
    CHECK(info1.cas_num_records == 0);

    // Resolving the request (running the task) should create an entry in the
    // CAS. The task holds a reference to the record-in-use while it runs, and
    // releases the reference when it finishes.
    auto res0 = cppcoro::sync_wait(task);
    REQUIRE(res0 == 9);
    auto info2{get_summary_info(mem_cache)};
    CHECK(info2.ac_num_records_in_use == 0);
    CHECK(info2.ac_num_records_pending_eviction == 1);
    CHECK(info2.cas_num_records == 1);

    // Deleting the task doesn't change anything.
    task = decltype(task)();
    auto info3{get_summary_info(mem_cache)};
    CHECK(info3.ac_num_records_in_use == 0);
    CHECK(info3.ac_num_records_pending_eviction == 1);
    CHECK(info3.cas_num_records == 1);
}

TEST_CASE("evaluate function request - lock cache record", tag)
{
    auto resources{make_inner_test_resources()};
    caching_request_resolution_context ctx{*resources};
    auto& mem_cache{resources->memory_cache()};
    request_props<caching_level_type::memory> props{make_test_uuid(10)};
    std::atomic<int> num_add_calls{};
    auto add{create_adder(num_add_calls)};
    auto req{rq_function(props, add, 6, 3)};

    // Resolve the request while obtaining a lock on the memory cache record.
    auto lock0{std::make_unique<cache_record_lock>()};
    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req, &*lock0));
    REQUIRE(res0 == 9);

    // Due to the lock, the AC record is still in use, and can't be evicted.
    clear_unused_entries(mem_cache);
    auto info0{get_summary_info(mem_cache)};
    CHECK(info0.ac_num_records_in_use == 1);
    CHECK(info0.ac_num_records_pending_eviction == 0);
    CHECK(info0.cas_num_records == 1);

    // Obtain a second lock on the same AC record.
    auto lock1{std::make_unique<cache_record_lock>()};
    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req, &*lock1));
    REQUIRE(res1 == 9);

    // The AC record now has two locks. Deleting one has no effect.
    lock0.reset();
    clear_unused_entries(mem_cache);
    auto info1{get_summary_info(mem_cache)};
    CHECK(info1.ac_num_records_in_use == 1);
    CHECK(info1.ac_num_records_pending_eviction == 0);
    CHECK(info1.cas_num_records == 1);

    // After all locks are gone, the AC record can be evicted.
    lock1.reset();
    clear_unused_entries(mem_cache);
    auto info2{get_summary_info(mem_cache)};
    CHECK(info2.ac_num_records_in_use == 0);
    CHECK(info2.ac_num_records_pending_eviction == 0);
    CHECK(info2.cas_num_records == 0);
}

template<caching_level_type Level, typename Ctx>
static void
test_composition_or_value_based()
{
    auto resources{make_inner_test_resources()};
    Ctx ctx{*resources};
    using props_type = request_props<Level>;
    props_type props_inner{make_test_uuid(100)};
    props_type props_main{make_test_uuid(101)};

    std::atomic<int> num_add_calls = 0;
    auto add = create_adder(num_add_calls);
    std::atomic<int> num_mul_calls = 0;
    auto mul = create_multiplier(num_mul_calls);

    auto inner0{rq_function(props_inner, add, 6, 8)};
    auto req0{rq_function(props_main, mul, inner0, 3)};
    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req0));
    REQUIRE(res0 == 42);
    REQUIRE(num_add_calls == 1);
    REQUIRE(num_mul_calls == 1);

    if constexpr (is_fully_cached(Level))
    {
        sync_wait_write_disk_cache(*resources);
        resources->reset_memory_cache();
    }

    auto inner1{rq_function(props_inner, add, 2, 12)};
    auto req1{rq_function(props_main, mul, inner1, 3)};
    auto res10 = cppcoro::sync_wait(resolve_request(ctx, req1));
    REQUIRE(res10 == 42);
    REQUIRE(num_add_calls == 2);
    // Value-based caching detects that the 14 * 3 result is already cached.
    REQUIRE(num_mul_calls == (is_value_based(Level) ? 1 : 2));

    if constexpr (is_fully_cached(Level))
    {
        sync_wait_write_disk_cache(*resources);
        resources->reset_memory_cache();
    }

    auto res11 = cppcoro::sync_wait(resolve_request(ctx, req1));
    REQUIRE(res11 == 42);
    REQUIRE(num_add_calls == 2);
    REQUIRE(num_mul_calls == (is_value_based(Level) ? 1 : 2));

    if constexpr (is_fully_cached(Level))
    {
        auto& disk_cache{
            static_cast<local_disk_cache&>(resources->secondary_cache())};
        auto dc = disk_cache.get_summary_info();
        // Composition-based has four AC entries, for
        // - 6+8
        // - (6+8)*3
        // - 2+12
        // - (2+12)*3
        // Value-based has three AC entries, for
        // - 6+8
        // - 14*3 (used for both requests)
        // - 2+12
        CHECK(dc.ac_entry_count == (is_value_based(Level) ? 3 : 4));
        CHECK(dc.cas_entry_count == 2);
    }
}

template<caching_level_type Level>
static void
test_composition_or_value_based_sync()
{
    return test_composition_or_value_based<
        Level,
        caching_request_resolution_context>();
}

TEST_CASE("evaluate function request - memory cached, CBC, sync", tag)
{
    test_composition_or_value_based_sync<caching_level_type::memory>();
}

TEST_CASE("evaluate function request - memory cached, VBC, sync", tag)
{
    test_composition_or_value_based_sync<caching_level_type::memory_vb>();
}

TEST_CASE("evaluate function request - disk cached, CBC, sync", tag)
{
    test_composition_or_value_based_sync<caching_level_type::full>();
}

TEST_CASE("evaluate function request - disk cached, VBC, sync", tag)
{
    test_composition_or_value_based_sync<caching_level_type::full_vb>();
}

template<caching_level_type Level>
static void
test_composition_or_value_based_async()
{
    return test_composition_or_value_based<Level, atst_context>();
}

TEST_CASE("evaluate function request - memory cached, CBC, async", tag)
{
    test_composition_or_value_based_async<caching_level_type::memory>();
}

TEST_CASE("evaluate function request - memory cached, VBC, async", tag)
{
    test_composition_or_value_based_async<caching_level_type::memory_vb>();
}

TEST_CASE("evaluate function request - disk cached, CBC, async", tag)
{
    test_composition_or_value_based_async<caching_level_type::full>();
}

TEST_CASE("evaluate function request - disk cached, VBC, async", tag)
{
    test_composition_or_value_based_async<caching_level_type::full_vb>();
}

// Verify that caches distinguish between plain blobs and blob files whose
// values are identical:
// Resolve a request to a plain blob and store it in the cache(s).
// Then resolve an almost identical request to a blob file and check that the
// result was really calculated, and not read from the cache.
template<caching_level_type caching_level>
static void
test_resolve_blob_file_or_not(std::string const& proxy_name)
{
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option{})};
    testing_request_context ctx{*resources, proxy_name};

    auto req0{rq_make_some_blob<caching_level>(256, false)};
    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req0));

    REQUIRE(res0.size() == 256);
    REQUIRE(res0.data()[0xff] == static_cast<std::byte>(0x55));
    auto* res0_owner = res0.mapped_file_data_owner();
    REQUIRE(res0_owner == nullptr);

    auto req1{rq_make_some_blob<caching_level>(256, true)};
    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req1));

    REQUIRE(res1.size() == 256);
    REQUIRE(res1.data()[0xff] == static_cast<std::byte>(0x55));
    auto* res1_owner = res1.mapped_file_data_owner();
    REQUIRE(res1_owner != nullptr);
}

TEST_CASE("resolve request - blob file or not - mem, local", tag)
{
    test_resolve_blob_file_or_not<caching_level_type::memory>("");
}

TEST_CASE("resolve request - blob file or not - mem, loopback", tag)
{
    test_resolve_blob_file_or_not<caching_level_type::memory>("loopback");
}

TEST_CASE("resolve request - blob file or not - mem, rpclib", tag)
{
    test_resolve_blob_file_or_not<caching_level_type::memory>("rpclib");
}

TEST_CASE("resolve request - blob file or not - full, local", tag)
{
    test_resolve_blob_file_or_not<caching_level_type::full>("");
}

TEST_CASE("resolve request - blob file or not - full, loopback", tag)
{
    test_resolve_blob_file_or_not<caching_level_type::full>("loopback");
}

TEST_CASE("resolve request - blob file or not - full, rpclib", tag)
{
    test_resolve_blob_file_or_not<caching_level_type::full>("rpclib");
}

// If test_remove_blob_file is:
// - false: verify that the cache stores a blob file by path, not by value
// - true: verify that the framework is robust against a removed blob file
//   (even if blob files shouldn't just disappear)
template<caching_level_type caching_level>
static void
test_resolve_to_blob_file(bool test_remove_blob_file)
{
    std::string proxy_name{};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option{})};
    testing_request_context ctx{*resources, proxy_name};

    auto req{rq_make_some_blob<caching_level>(256, true)};
    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(res0.size() == 256);
    REQUIRE(res0.data()[0xff] == static_cast<std::byte>(0x55));
    auto* res0_owner = res0.mapped_file_data_owner();
    REQUIRE(res0_owner != nullptr);
    std::string file0{res0_owner->mapped_file()};

    if (test_remove_blob_file)
    {
        remove(file_path(file0));
    }
    if constexpr (is_fully_cached(caching_level))
    {
        sync_wait_write_disk_cache(*resources);
        resources->reset_memory_cache();
    }

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(res1.size() == 256);
    REQUIRE(res1.data()[0xff] == static_cast<std::byte>(0x55));
    auto* res1_owner = res1.mapped_file_data_owner();
    REQUIRE(res1_owner != nullptr);
    std::string file1{res1_owner->mapped_file()};
    if (!test_remove_blob_file)
    {
        // The second resolve should return the cached blob file.
        REQUIRE(file1 == file0);
    }
    else
    {
        if constexpr (!is_fully_cached(caching_level))
        {
            // The memory cache entry should hold on to the original shared
            // memory region, even though that can no longer be accessed via
            // the removed blob file.
            REQUIRE(file1 == file0);
        }
        else
        {
            // The disk cache cannot hold on to the shared memory region, so
            // the second resolve should have created a new blob file.
            REQUIRE(file1 != file0);
        }
    }
}

TEST_CASE("resolve request - blob file storage in cache - mem", tag)
{
    test_resolve_to_blob_file<caching_level_type::memory>(false);
}

TEST_CASE("resolve request - blob file storage in cache - full", tag)
{
    test_resolve_to_blob_file<caching_level_type::full>(false);
}

TEST_CASE("resolve request - disappearing blob file - mem", tag)
{
    test_resolve_to_blob_file<caching_level_type::memory>(true);
}

TEST_CASE("resolve request - disappearing blob file - full", tag)
{
    test_resolve_to_blob_file<caching_level_type::full>(true);
}

// Ctx should be non-introspective
template<caching_level_type req_level, typename Ctx>
static void
test_intrsp_req_bad_ctx()
{
    auto req{rq_make_some_blob<req_level>(256, false)};
    auto resources{make_inner_test_resources()};
    Ctx ctx{*resources};
    context_intf& ctx_intf{ctx};

    // resolve_request() should fail due to mismatch between req and ctx:
    // req is introspective, ctx is not.
    // Pass ctx_intf, not ctx itself, to force a runtime mismatch.
    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx_intf, req)),
        "failing cast_ctx_to_ref");
}

TEST_CASE("resolve request - cached intrsp req, non-intrsp ctx", tag)
{
    test_intrsp_req_bad_ctx<
        caching_level_type::memory,
        caching_request_resolution_context>();
}

#if 0
// TODO add introspection to resolve_request_sync_uncached()
TEST_CASE("resolve request - uncached intrsp req, non-intrsp ctx", tag)
{
    test_intrsp_req_bad_ctx<
        caching_level_type::none,
        non_caching_request_resolution_context>();
}
#endif

TEST_CASE("resolve request - cached req, uncached ctx", tag)
{
    constexpr auto caching_level = caching_level_type::memory;
    auto resources{make_inner_test_resources()};

    // req is cached
    auto req{rq_make_some_blob<caching_level>(256, false)};

    // ctx is uncached
    non_caching_request_resolution_context ctx{*resources};
    context_intf& ctx_intf{ctx};

    // resolve_request() should fail due to mismatch between req and ctx
    // Pass ctx_intf, not ctx itself, to force a runtime mismatch.
    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx_intf, req)),
        "failing cast_ctx_to_ref");
}

template<typename props_type, typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
static auto
rq_200x(int uuid_ext, A a, B b)
{
    return rq_function(
        props_type{make_test_uuid(uuid_ext)},
        add2,
        normalize_arg<int, props_type>(std::move(a)),
        normalize_arg<int, props_type>(std::move(b)));
}

// Different request properties in a tree are possible. The only restriction is
// that the functions all are plain, or all are coroutines. (One reason being
// that a coroutine takes a context_intf& parameter by convention, and a plain
// function does not.)
TEST_CASE("resolve request tree with different props", tag)
{
    auto resources{make_inner_test_resources()};
    auto& mem_cache{resources->memory_cache()};
    auto& disk_cache{
        static_cast<local_disk_cache&>(resources->secondary_cache())};
    caching_request_resolution_context ctx{*resources};

    using none_props = request_props<caching_level_type::none>;
    using mem_props = request_props<caching_level_type::memory>;
    using full_props = request_props<caching_level_type::full>;
    auto req_a{rq_200x<mem_props>(2000, 1, 2)};
    auto req_b{rq_200x<none_props>(2001, req_a, 4)};
    auto req_c{rq_200x<full_props>(2002, 8, req_b)};

    // Resolve while the caches do not contain anything relevant
    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req_c));
    sync_wait_write_disk_cache(*resources);
    REQUIRE(res0 == 15);
    auto mi0 = get_summary_info(mem_cache);
    CHECK(mi0.hit_count == 0);
    // Expecting a miss for the two cached requests (req_a, req_c)
    CHECK(mi0.miss_count == 2);
    auto di0 = disk_cache.get_summary_info();
    CHECK(di0.hit_count == 0);
    // Expecting a miss for the one disk-cached request (req_c)
    CHECK(di0.miss_count == 1);

    // Resolve via the memory cache
    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req_c));
    REQUIRE(res1 == 15);
    auto mi1 = get_summary_info(mem_cache);
    CHECK(mi1.hit_count == 1);
    CHECK(mi1.miss_count == 2);
    auto di1 = disk_cache.get_summary_info();
    CHECK(di1.hit_count == 0);
    CHECK(di1.miss_count == 1);

    // Resolve via the disk cache
    resources->reset_memory_cache();
    auto res2 = cppcoro::sync_wait(resolve_request(ctx, req_c));
    REQUIRE(res2 == 15);
    auto mi2 = get_summary_info(mem_cache);
    CHECK(mi2.hit_count == 0);
    // Expecting a miss for req_c only; as there is a hit on the disk cache,
    // no attempt is made to resolve any subrequest.
    CHECK(mi2.miss_count == 1);
    auto di2 = disk_cache.get_summary_info();
    CHECK(di2.hit_count == 1);
    CHECK(di2.miss_count == 1);
}

static void
test_resolve_inner_blob_file(bool allow_blob_files)
{
    constexpr auto caching_level{caching_level_type::full};
    constexpr bool use_shared_memory{true};
    std::string proxy_name{""};
    inner_resources resources{make_inner_tests_config()};
    std::unique_ptr<secondary_storage_intf> storage;
    if (allow_blob_files)
    {
        storage = std::make_unique<simple_blob_storage>();
    }
    else
    {
        storage = std::make_unique<simple_string_storage>();
    }
    resources.set_secondary_cache(std::move(storage));
    testing_request_context ctx{resources, proxy_name};
    ctx.track_blob_file_writers();

    auto req{rq_make_demo_class<caching_level>(
        3, make_test_blob(ctx, "abc", use_shared_memory))};
    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req));
    CHECK(res0.get_x() == 3);
    CHECK(to_string(res0.get_y()) == "abc");
    auto y0_owner = res0.get_y().mapped_file_data_owner();
    // The result has been calculated, serialized for secondary storage, and
    // deserialized into res0. So if secondary storage disallows blob files,
    // res0 won't have any.
    CHECK((y0_owner != nullptr) == allow_blob_files);

    resources.reset_memory_cache();

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req));
    CHECK(res1.get_x() == 3);
    CHECK(to_string(res1.get_y()) == "abc");
    auto y1_owner = res1.get_y().mapped_file_data_owner();
    CHECK((y1_owner != nullptr) == allow_blob_files);
}

TEST_CASE("resolve inner blob file - full - allow", tag)
{
    test_resolve_inner_blob_file(true);
}

TEST_CASE("resolve inner blob file - full - disallow", tag)
{
    test_resolve_inner_blob_file(false);
}

static void
test_resolve_outer_blob_file(bool allow_blob_files)
{
    constexpr auto caching_level{caching_level_type::full};
    constexpr bool use_shared_memory{true};
    std::string proxy_name{""};
    inner_resources resources{make_inner_tests_config()};
    std::unique_ptr<secondary_storage_intf> storage;
    if (allow_blob_files)
    {
        storage = std::make_unique<simple_blob_storage>();
    }
    else
    {
        storage = std::make_unique<simple_string_storage>();
    }
    resources.set_secondary_cache(std::move(storage));
    testing_request_context ctx{resources, proxy_name};
    ctx.track_blob_file_writers();

    auto req{rq_make_some_blob<caching_level>(256, use_shared_memory)};
    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req));
    CHECK(res0.size() == 256);
    CHECK(res0.data()[0xff] == static_cast<std::byte>(0x55));
    auto* res0_owner = res0.mapped_file_data_owner();
    // Even if the cache disallows blob files, serializing a blob returns that
    // blob; so the resolve_request() result always is a blob file.
    CHECK(res0_owner != nullptr);

    resources.reset_memory_cache();

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req));
    CHECK(res1.size() == 256);
    CHECK(res1.data()[0xff] == static_cast<std::byte>(0x55));
    auto* res1_owner = res1.mapped_file_data_owner();
    // This time the result comes from the cache, where the blob was stored in
    // expanded form (a byte sequence containing no reference to the blob
    // file).
    CHECK((res1_owner != nullptr) == allow_blob_files);
}

TEST_CASE("resolve outer blob file - full - allow", tag)
{
    test_resolve_outer_blob_file(true);
}

TEST_CASE("resolve outer blob file - full - disallow", tag)
{
    test_resolve_outer_blob_file(false);
}
