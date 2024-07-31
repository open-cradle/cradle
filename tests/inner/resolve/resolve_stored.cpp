#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../support/inner_service.h"
#include <cradle/inner/service/request_store.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/testing_seri_catalog.h>
#include <cradle/plugins/secondary_cache/simple/simple_storage.h>

namespace cradle {

#define TAG(n) "[inner][resolve][resolve_stored][" #n "]"

namespace {

void
test_store_load_resolve(Request auto& req_written, int expected)
{
    std::string const proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    resources->set_requests_storage(std::make_unique<simple_blob_storage>());
    testing_seri_catalog cat{resources->get_seri_registry()};

    cppcoro::sync_wait(store_request(req_written, *resources));

    std::string key{get_request_key(req_written)};
    using Req = std::remove_cvref_t<decltype(req_written)>;
    auto req_read = cppcoro::sync_wait(load_request<Req>(key, *resources));
    CHECK(req_read == req_written);

    atst_context ctx{*resources, proxy_name};
    auto res = cppcoro::sync_wait(resolve_request(ctx, req_read));
    REQUIRE(res == expected);
}

} // namespace

TEST_CASE("store/load/resolve function_request to/from storage", TAG(10))
{
    constexpr int loops = 3;
    int delay0 = 5;
    int delay1 = 30;
    int expected = (loops + delay0) + (loops + delay1);

    constexpr auto level = caching_level_type::memory;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0),
        rq_cancellable_coro<level>(loops, delay1))};

    test_store_load_resolve(req, expected);
}

TEST_CASE("store/load/resolve proxy_request to/from storage", TAG(11))
{
    constexpr int loops = 3;
    int delay0 = 4;
    int delay1 = 50;
    int expected = (loops + delay0) + (loops + delay1);

    constexpr auto level = caching_level_type::memory;
    auto req{rq_cancellable_proxy<level>(
        rq_cancellable_proxy<level>(loops, delay0),
        rq_cancellable_proxy<level>(loops, delay1))};

    test_store_load_resolve(req, expected);
}

} // namespace cradle
