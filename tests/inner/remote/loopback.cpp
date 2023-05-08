#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/domain.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>

#include "../../support/inner_service.h"

using namespace cradle;

namespace {

static char const tag[] = "[inner][remote][loopback]";

void
test_make_some_blob(bool async, bool shared)
{
#if 0
    constexpr auto caching_level{caching_level_type::full};
    constexpr auto remotely{true};
    auto dom{find_domain("testing")};
    std::string proxy_name{"loopback"};
    register_testing_seri_resolvers();
    inner_resources resources;
    init_test_inner_service(resources);
    ensure_loopback_service(resources);

    auto req{rq_make_some_blob<caching_level>(10000, shared)};
    blob response;
    if (!async)
    {
        auto ctx = dom->make_sync_context(resources, remotely);
        auto& rem_ctx = to_remote_ref(*ctx);
        rem_ctx.proxy_name(proxy_name);
        //testing_request_context ctx{resources, nullptr, remotely};
        response = cppcoro::sync_wait(resolve_request(*ctx, req));
    }
    else
    {
        // TODO make_some_blob expects a testing_request_context, but the local counterpart of proxy_atst_tree_context is local_atst_tree_context
        // TODO merge the "testing" domain's sync and async contexts?
        // TODO loopback needs async_db; let it ensure
        resources.ensure_async_db();
        auto ctx = dom->make_async_context(resources, remotely);
        auto& rem_ctx = to_remote_ref(*ctx);
        rem_ctx.proxy_name(proxy_name);
        //auto tree_ctx{std::make_shared<proxy_atst_tree_context>(resources, proxy_name)};
        //proxy_atst_context ctx{tree_ctx, true, true};
        response = cppcoro::sync_wait(resolve_request(*ctx, req));
    }

    REQUIRE(response.size() == 10000);
    REQUIRE(response.data()[0xff] == static_cast<std::byte>(0x55));
    REQUIRE(response.data()[9999] == static_cast<std::byte>(0x35));
#endif
}

} // namespace

// TODO loopback tests with complex request

TEST_CASE("loopback: make some plain blob, sync", tag)
{
    test_make_some_blob(false, false);
}

TEST_CASE("loopback: make some blob file, sync", tag)
{
    test_make_some_blob(false, true);
}

TEST_CASE("loopback: make some plain blob, async", "[X]")
{
    test_make_some_blob(true, false);
}

TEST_CASE("loopback: make some blob file, async", tag)
{
    test_make_some_blob(true, true);
}
