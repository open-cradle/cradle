#include <catch2/catch.hpp>

#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/proxy_impl.h>

#include "../support/inner_service.h"

using namespace cradle;

TEST_CASE("git version mismatch", "[rpclib]")
{
    auto client = ensure_rpclib_service();
    auto& impl{client->pimpl()};

    REQUIRE_THROWS_AS(impl.verify_git_version("bad version"), rpclib_error);
}
