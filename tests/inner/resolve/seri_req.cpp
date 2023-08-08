#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/resolve/seri_req.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/domain.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>
#include <cradle/plugins/serialization/request/cereal_json.h>
#include <cradle/plugins/serialization/response/msgpack.h>

#include "../../support/inner_service.h"

using namespace cradle;

static char const tag[] = "[inner][resolve][seri_req]";

static void
test_resolve(bool remotely)
{
    register_and_initialize_testing_domain();
    register_testing_seri_resolvers();
    inner_resources resources;
    init_test_inner_service(resources);
    if (remotely)
    {
        register_loopback_service(make_inner_tests_config(), resources);
    }
    testing_request_context ctx{resources, nullptr, remotely, "loopback"};

    constexpr auto caching_level{caching_level_type::full};
    auto req{rq_make_some_blob<caching_level>(256, false)};
    std::string seri_req{serialize_request(req)};
    auto seri_resp
        = cppcoro::sync_wait(resolve_serialized_request(ctx, seri_req));
    blob response = deserialize_response<blob>(seri_resp.value());
    seri_resp.on_deserialized();

    REQUIRE(response.size() == 256);
    REQUIRE(response.data()[0xff] == static_cast<std::byte>(0x55));
}

TEST_CASE("resolve serialized request, locally", tag)
{
    test_resolve(false);
}

TEST_CASE("resolve serialized request, remotely", tag)
{
    test_resolve(true);
}
