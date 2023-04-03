#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>

#include "../../support/inner_service.h"

using namespace cradle;

namespace {

void
test_make_some_blob(bool shared)
{
    constexpr auto caching_level{caching_level_type::full};
    constexpr auto remotely{true};
    std::string proxy_name{"loopback"};
    register_testing_seri_resolvers();
    ensure_loopback_service();
    inner_resources service;
    init_test_inner_service(service);
    testing_request_context ctx{service, nullptr, remotely};
    ctx.proxy_name(proxy_name);

    auto req{rq_make_some_blob<caching_level>(10000, shared)};
    auto response = cppcoro::sync_wait(resolve_request(ctx, req));

    REQUIRE(response.size() == 10000);
    REQUIRE(response.data()[0xff] == static_cast<std::byte>(0x55));
    REQUIRE(response.data()[9999] == static_cast<std::byte>(0x35));
}

} // namespace

TEST_CASE("plain blob across loopback", "[inner][remote]")
{
    test_make_some_blob(false);
}

TEST_CASE("blob file across loopback", "[inner][remote]")
{
    test_make_some_blob(true);
}
