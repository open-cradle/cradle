#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../inner-dll/v1/adder_v1.h"
#include "../../inner-dll/x0/adder_x0.h"
#include "../../inner-dll/x1/multiplier_x1.h"
#include "../../support/inner_service.h"
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/rpclib/client/registry.h>
#include <cradle/test_dlls_dir.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][resolve][proxy]";

} // namespace

TEST_CASE("evaluate proxy request, plain args", tag)
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto& proxy = register_rpclib_client(make_inner_tests_config(), resources);
    proxy.unload_shared_library("test_inner_dll_v1.*");

    auto req{rq_test_adder_v1p(7, 2)};
    int expected{7 + 2};

    tasklet_tracker* tasklet{nullptr};
    bool remotely{true};
    testing_request_context ctx{resources, tasklet, remotely, "rpclib"};
    ResolutionConstraintsRemoteSync constraints;

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req, constraints)),
        Catch::Contains("no resolver registered for uuid"));

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res == expected);
}

TEST_CASE("evaluate proxy request, normalized args", tag)
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto& proxy = register_rpclib_client(make_inner_tests_config(), resources);
    proxy.unload_shared_library("test_inner_dll_v1.*");

    auto req{rq_test_adder_v1n(7, 2)};
    int expected{7 + 2};

    tasklet_tracker* tasklet{nullptr};
    bool remotely{true};
    testing_request_context ctx{resources, tasklet, remotely, "rpclib"};
    ResolutionConstraintsRemoteSync constraints;

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req, constraints)),
        Catch::Contains("no resolver registered for uuid"));

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res == expected);
}

// The test_inner_dll_x0 and test_inner_dll_x1 DLLs each define one request.
// The two request functions have the same signature, so the corresponding
// function_request_impl instantiations will be identical too. Depending on the
// dynamic loader implementation, the two functions use the same, global,
// function_request_impl::matching_functions_ singleton (Linux), or a per-DLL
// one (Windows). Both should work correctly.
TEST_CASE("two DLLs defining same-typed requests", tag)
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto& proxy = register_rpclib_client(make_inner_tests_config(), resources);
    proxy.unload_shared_library("test_inner_dll_x.*");
    tasklet_tracker* tasklet{nullptr};
    bool remotely{true};
    testing_request_context ctx{resources, tasklet, remotely, "rpclib"};
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
    inner_resources resources;
    init_test_inner_service(resources);
    auto config{make_inner_tests_config()};
    auto& proxy = register_rpclib_client(config, resources);
    proxy.unload_shared_library("test_inner_dll_v1.*");

    auto req{rq_test_adder_v1p(7, 2)};
    int expected{7 + 2};

    tasklet_tracker* tasklet{nullptr};
    bool remotely{true};
    testing_request_context ctx{resources, tasklet, remotely, "rpclib"};
    ResolutionConstraintsRemoteSync constraints;

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res0 == expected);

    proxy.unload_shared_library("test_inner_dll_v1.*");

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req, constraints)),
        Catch::Contains("no resolver registered for uuid"));

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res1 == expected);
}

TEST_CASE("unload/reload two DLLs", tag)
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto& proxy = register_rpclib_client(make_inner_tests_config(), resources);
    proxy.unload_shared_library("test_inner_dll_x.*");
    tasklet_tracker* tasklet{nullptr};
    bool remotely{true};
    testing_request_context ctx{resources, tasklet, remotely, "rpclib"};
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

#if 0
// Currently failing since unloading a DLL unregisters _all_ resolvers for its DLLs
TEST_CASE("unload DLL sharing resolvers", "[B]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto& proxy = register_rpclib_client(make_inner_tests_config(), resources);
    proxy.unload_shared_library("test_inner_dll_x.*");
    tasklet_tracker* tasklet{nullptr};
    bool remotely{true};
    testing_request_context ctx{resources, tasklet, remotely, "rpclib"};
    ResolutionConstraintsRemoteSync constraints;

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x0x1");
    // TODO next line fails with
    // "Multiple C++ functions for uuid test-adder-x0"
    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x0");
    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_x1");

    auto add_req{rq_test_adder_x0(7, 2)};
    int add_expected{7 + 2};
    auto mul_req{rq_test_multiplier_x1(7, 2)};
    int mul_expected{7 * 2};

    auto add_actual0
        = cppcoro::sync_wait(resolve_request(ctx, add_req, constraints));
    REQUIRE(add_actual0 == add_expected);
    auto mul_actual0
        = cppcoro::sync_wait(resolve_request(ctx, mul_req, constraints));
    REQUIRE(mul_actual0 == mul_expected);

    proxy.unload_shared_library("test_inner_dll_x0x1");

    // TODO if we get here, next line fails with
    // "no resolver registered for uuid test-adder-x0"
    auto add_actual1
        = cppcoro::sync_wait(resolve_request(ctx, add_req, constraints));
    REQUIRE(add_actual1 == add_expected);
    auto mul_actual1
        = cppcoro::sync_wait(resolve_request(ctx, mul_req, constraints));
    REQUIRE(mul_actual1 == mul_expected);
}

TEST_CASE("load/unload DLL stress test", "[Z]")
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto& proxy = register_rpclib_client(make_inner_tests_config(), resources);
    proxy.unload_shared_library("test_inner_dll_x.*");

    std::srand(std::time(nullptr));
    std::vector<std::string> dll_names{
        "test_inner_dll_x0",
        "test_inner_dll_x1",
        "test_inner_dll_x0x1"
    };
    for ( ; ; )
    {
        int i = random() % 6;
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
#endif

TEST_CASE("load/unload DLL stress test1", tag)
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto& proxy = register_rpclib_client(make_inner_tests_config(), resources);
    proxy.unload_shared_library("test_inner_dll_x.*");
    tasklet_tracker* tasklet{nullptr};
    bool remotely{true};
    testing_request_context ctx{resources, tasklet, remotely, "rpclib"};
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
