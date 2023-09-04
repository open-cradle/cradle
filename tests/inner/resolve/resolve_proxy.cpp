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

TEST_CASE("evaluate proxy request", tag)
{
    inner_resources resources;
    init_test_inner_service(resources);
    auto& proxy = register_rpclib_client(make_inner_tests_config(), resources);

    auto req{rq_test_adder_v1(7, 2)};
    int expected{7 + 2};

    tasklet_tracker* tasklet{nullptr};
    bool remotely{true};
    testing_request_context ctx{resources, tasklet, remotely, "rpclib"};
    ResolutionConstraintsRemoteSync constraints;

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req, constraints)),
        Catch::Contains("no resolver registered for uuid"));

    proxy.load_shared_library(get_test_dlls_dir(), "test_inner_dll_v1");

    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req, constraints));
    REQUIRE(res1 == expected);
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
