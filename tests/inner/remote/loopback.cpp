#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/domain.h>
#include <cradle/plugins/domain/testing/domain_factory.h>
#include <cradle/plugins/domain/testing/requests.h>

#include "../../support/inner_service.h"

using namespace cradle;

namespace {

static char const tag[] = "[inner][remote][loopback]";

void
test_make_some_blob(bool async, bool shared)
{
    constexpr auto caching_level{caching_level_type::full};
    constexpr auto remotely{true};
    std::string proxy_name{"loopback"};
    auto resources{make_inner_test_resources()};
    init_test_loopback_service(resources, true);

    auto req{rq_make_some_blob<caching_level>(10000, shared)};
    blob response;
    if (!async)
    {
        testing_request_context ctx{resources, nullptr, remotely, proxy_name};
        ResolutionConstraintsRemoteSync constraints;
        response = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    }
    else
    {
        auto tree_ctx{
            std::make_shared<proxy_atst_tree_context>(resources, proxy_name)};
        root_proxy_atst_context ctx{tree_ctx};
        ResolutionConstraintsRemoteAsync constraints;
        response = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    }

    REQUIRE(response.size() == 10000);
    REQUIRE(response.data()[0xff] == static_cast<std::byte>(0x55));
    REQUIRE(response.data()[9999] == static_cast<std::byte>(0x35));
}

} // namespace

// TODO loopback tests with complex request (already have this in async.cpp?)

TEST_CASE("loopback: make some plain blob, sync", tag)
{
    test_make_some_blob(false, false);
}

TEST_CASE("loopback: make some blob file, sync", tag)
{
    test_make_some_blob(false, true);
}

TEST_CASE("loopback: make some plain blob, async", tag)
{
    test_make_some_blob(true, false);
}

TEST_CASE("loopback: make some blob file, async", tag)
{
    test_make_some_blob(true, true);
}
