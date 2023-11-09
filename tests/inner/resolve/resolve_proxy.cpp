#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../inner-dll/v1/adder_v1.h"
#include "../../inner-dll/x0/adder_x0.h"
#include "../../inner-dll/x1/multiplier_x1.h"
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

// The test_inner_dll_x0 and test_inner_dll_x1 DLLs each define one request.
// The two request functions have the same signature, so the corresponding
// function_request_impl instantiations will be identical too. The framework
// should distinguish the two.
TEST_CASE("two DLLs defining same-typed requests", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& proxy{resources->get_proxy(proxy_name)};
    proxy.unload_shared_library("test_inner_dll_x.*");
    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};
    ResolutionConstraintsRemoteSync constraints;

    auto add_req{rq_test_adder_x0(7, 2)};
    int add_expected{7 + 2};
    auto mul_req{rq_test_multiplier_x1(7, 2)};
    int mul_expected{7 * 2};

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x0");
    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x1");

    auto add_actual
        = cppcoro::sync_wait(resolve_request(ctx, add_req, constraints));
    REQUIRE(add_actual == add_expected);
    auto mul_actual
        = cppcoro::sync_wait(resolve_request(ctx, mul_req, constraints));
    REQUIRE(mul_actual == mul_expected);
}

TEST_CASE("unload/reload DLL", tag)
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

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res0 == expected);

    proxy.unload_shared_library("test_inner_dll_v1.*");

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req, constraints)),
        Catch::Contains("no entry found for uuid"));

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res1 == expected);
}

TEST_CASE("unload/reload two DLLs", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& proxy{resources->get_proxy(proxy_name)};
    proxy.unload_shared_library("test_inner_dll_x.*");
    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};
    ResolutionConstraintsRemoteSync constraints;

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x0");
    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x1");
    proxy.unload_shared_library("test_inner_dll_x.*");

    auto add_req{rq_test_adder_x0(7, 2)};
    int add_expected{7 + 2};
    auto mul_req{rq_test_multiplier_x1(7, 2)};
    int mul_expected{7 * 2};

    // Insert another DLL to increase the chance that x0 and x1 are loaded
    // at different addresses than before.
    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x0");
    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x1");

    auto add_actual
        = cppcoro::sync_wait(resolve_request(ctx, add_req, constraints));
    REQUIRE(add_actual == add_expected);
    auto mul_actual
        = cppcoro::sync_wait(resolve_request(ctx, mul_req, constraints));
    REQUIRE(mul_actual == mul_expected);
}

TEST_CASE("unload DLL sharing resolvers", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& proxy{resources->get_proxy(proxy_name)};
    proxy.unload_shared_library("test_inner_dll_x.*");
    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};
    ResolutionConstraintsRemoteSync constraints;

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x0x1");
    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x0");
    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x1");

    auto add_req{rq_test_adder_x0(7, 2)};
    int add_expected{7 + 2};
    auto mul_req{rq_test_multiplier_x1(7, 2)};
    int mul_expected{7 * 2};

    // The "add" resolver is in x0 and x0x1.
    // The "mul" resolver is in x1 and x0x1.
    auto add_actual0
        = cppcoro::sync_wait(resolve_request(ctx, add_req, constraints));
    REQUIRE(add_actual0 == add_expected);
    auto mul_actual0
        = cppcoro::sync_wait(resolve_request(ctx, mul_req, constraints));
    REQUIRE(mul_actual0 == mul_expected);

    proxy.unload_shared_library("test_inner_dll_x0x1");

    // x0x1 has been unloaded, but:
    // - The "add" resolver still is in x0.
    // - The "mul" resolver still is in x1.
    // So both resolutions still should succeed.
    auto add_actual1
        = cppcoro::sync_wait(resolve_request(ctx, add_req, constraints));
    REQUIRE(add_actual1 == add_expected);
    auto mul_actual1
        = cppcoro::sync_wait(resolve_request(ctx, mul_req, constraints));
    REQUIRE(mul_actual1 == mul_expected);
}

// Manual stress test, disabled by default.
// This one used to cause crashes.
TEST_CASE("load/unload DLL stress test", "[.dll-stress]")
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& proxy{resources->get_proxy(proxy_name)};
    proxy.unload_shared_library("test_inner_dll_x.*");

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::vector<std::string> dll_names{
        "test_inner_dll_x0",
        "test_inner_dll_x1",
        "test_inner_dll_x0x1"
    };
    for ( ; ; )
    {
        int i = std::rand() % 6;
        auto const& dll_name{dll_names[i / 2]};
        std::string action;
        try
        {
            if (i % 2 == 0)
            {
                action = "load";
                proxy.load_shared_library(get_test_dlls_dir(), dll_name);
            }
            else
            {
                action = "unload";
                proxy.unload_shared_library(dll_name);
            }
        }
        catch (std::exception const& e)
        {
            fmt::print("{} DLL {} failed: {}\n", action, dll_name, e.what());
        }
    }
}

TEST_CASE("load/unload DLL stress test1", tag)
{
    std::string proxy_name{"rpclib"};
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    auto& proxy{resources->get_proxy(proxy_name)};
    proxy.unload_shared_library("test_inner_dll_x.*");
    tasklet_tracker* tasklet{nullptr};
    testing_request_context ctx{*resources, tasklet, proxy_name};
    ResolutionConstraintsRemoteSync constraints;

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x0");
    try
    {
        proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x0x1");
    }
    catch (...)
    {
    }

    auto add_req{rq_test_adder_x0(7, 2)};
    int add_expected{7 + 2};
    auto add_actual
        = cppcoro::sync_wait(resolve_request(ctx, add_req, constraints));
    REQUIRE(add_actual == add_expected);

    proxy.unload_shared_library("test_inner_dll_x0");
}
