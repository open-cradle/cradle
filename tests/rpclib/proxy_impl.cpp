#include <catch2/catch.hpp>

#include <cradle/inner/service/resources.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/proxy_impl.h>
#include <cradle/rpclib/client/registry.h>

#include "../support/inner_service.h"

using namespace cradle;

TEST_CASE("rpclib protocol mismatch", "[rpclib]")
{
    auto resources{make_inner_test_resources()};
    auto& client
        = register_rpclib_client(make_inner_tests_config(), resources);
    auto& impl{client.pimpl()};

    REQUIRE_THROWS_AS(
        impl.verify_rpclib_protocol("incompatible"), remote_error);
}
