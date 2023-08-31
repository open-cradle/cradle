#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../inner-dll/v1/adder_v1.h"
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
