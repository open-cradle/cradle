#include <stdexcept>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../inner-dll/v1/adder_v1.h"
#include "../../support/inner_service.h"
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/test_dlls_dir.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][resolve][proxy]";

} // namespace

TEST_CASE("evaluate proxy request, plain args", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& proxy{resources->get_proxy(proxy_name)};
    proxy.unload_shared_library("test_inner_dll_v1.*");

    auto req{rq_test_adder_v1p(7, 2)};
    int expected{7 + 2};

    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};
    ResolutionConstraintsRemoteSync constraints;

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req, constraints)),
        Catch::Contains("no entry found for uuid"));

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res == expected);
}

TEST_CASE("evaluate proxy request, normalized args", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& proxy{resources->get_proxy(proxy_name)};
    proxy.unload_shared_library("test_inner_dll_v1.*");

    auto req{rq_test_adder_v1n(7, 2)};
    int expected{7 + 2};

    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};
    ResolutionConstraintsRemoteSync constraints;

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req, constraints)),
        Catch::Contains("no entry found for uuid"));

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res == expected);
}

TEST_CASE("attempt to resolve proxy request locally", tag)
{
    std::string proxy_name{""};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};

    auto req{rq_test_adder_v1p(7, 2)};

    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};

    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(resolve_request(ctx, req)), std::logic_error);
}
