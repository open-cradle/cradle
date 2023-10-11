#include <catch2/catch.hpp>

#include <cradle/inner/service/resources.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/proxy_impl.h>

#include "../support/inner_service.h"

using namespace cradle;

TEST_CASE("rpclib protocol mismatch", "[rpclib]")
{
    std::string proxy_name{"rpclib"};
    auto resources{make_inner_test_resources(proxy_name)};
    auto& client{static_cast<rpclib_client&>(resources.get_proxy(proxy_name))};
    auto& impl{client.pimpl()};

    REQUIRE_THROWS_AS(
        impl.verify_rpclib_protocol("incompatible"), remote_error);
}
