#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../inner-dll/v1/adder_v1.h"
#include "../../support/inner_service.h"
#include <cradle/inner/dll/dll_collection.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/resolve/seri_req.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/domain_factory.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/testing_seri_catalog.h>
#include <cradle/plugins/serialization/response/msgpack.h>
#include <cradle/test_dlls_dir.h>

using namespace cradle;

static char const tag[] = "[inner][resolve][seri_req]";

static void
test_resolve(std::string const& proxy_name)
{
    auto resources{
        make_inner_test_resources(proxy_name, testing_domain_option())};
    testing_request_context ctx{*resources, nullptr, proxy_name};

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
    test_resolve("");
}

TEST_CASE("resolve serialized request, loopback", tag)
{
    test_resolve("loopback");
}

TEST_CASE("resolve serialized request, DLL", tag)
{
    std::string proxy_name{""};
    auto resources{make_inner_test_resources(proxy_name, no_domain_option())};
    tasklet_tracker* tasklet{nullptr};
    // TODO why testing_request_context?
    testing_request_context ctx{*resources, tasklet, proxy_name};

    auto req{rq_test_adder_v1p(7, 2)};
    int expected{7 + 2};
    std::string seri_req{serialize_request(req)};

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_serialized_request(ctx, seri_req)),
        Catch::Contains("no entry found for uuid"));

    std::string dll_name{"test_inner_dll_v1"};
    auto& the_dlls{resources->the_dlls()};
    the_dlls.load(get_test_dlls_dir(), dll_name);

    auto seri_resp
        = cppcoro::sync_wait(resolve_serialized_request(ctx, seri_req));
    int response = deserialize_response<int>(seri_resp.value());
    seri_resp.on_deserialized();

    REQUIRE(response == expected);

    the_dlls.unload(dll_name);

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_serialized_request(ctx, seri_req)),
        Catch::Contains("no entry found for uuid"));
}
