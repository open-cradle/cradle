#include "../../support/outer_service.h"
#include "../../support/thinknode.h"
#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/dll/shared_library.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/inner/remote/loopback.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/thinknode/context.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/thinknode/types.hpp>
#include <cradle/thinknode_dlls_dir.h>

using namespace cradle;

namespace {

static char const tag[] = "[thinknode][iss_dll]";

template<typename Request>
void
test_post_iss_request_loopback(Request const& request)
{
    service_core service;
    init_test_service(service);
    register_loopback_service(make_outer_tests_config(), service);
    ensure_all_domains_registered();

    load_shared_library(get_thinknode_dlls_dir(), "cradle_thinknode_v2");

    mock_http_exchange exchange{
        make_http_request(
            http_request_method::POST,
            "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
            {{"Authorization", "Bearer xyz"},
             {"Accept", "application/json"},
             {"Content-Type", "application/octet-stream"}},
            make_blob("payload_ijk")),
        make_http_200_response("{ \"id\": \"result_ijk\" }")};
    mock_http_script script;
    script.push_back(exchange);
    auto& mock_http = enable_http_mocking(service);
    mock_http.set_script(script);

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    tasklet_tracker* tasklet = nullptr;
    bool remotely{true};
    thinknode_request_context ctx{
        service, session, tasklet, remotely, "loopback"};

    auto actual0 = cppcoro::sync_wait(resolve_request(ctx, request));

    REQUIRE(actual0 == "result_ijk");
    REQUIRE(mock_http.is_complete());

    // This next one should come from the memory cache
    // (so the mock http sees no new request).
    auto actual1 = cppcoro::sync_wait(resolve_request(ctx, request));

    REQUIRE(actual1 == "result_ijk");

    unload_shared_library("cradle_thinknode_v2");

    // The result is still in the memory cache, but it won't retrieved as the
    // loopback service cannot find a resolver anymore.
    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(resolve_request(ctx, request)),
        unregistered_uuid_error);
}

} // namespace

TEST_CASE("ISS POST resolved via DLL / proxy / loopback", tag)
{
    auto req{rq_post_iss_object_v2(
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload_ijk"))};

    test_post_iss_request_loopback(req);
}

// rq_post_iss_object_v2 proxy DLL loopback
// rq_post_iss_object_v2 proxy DLL rpclib
// rq_post_iss_object_v2 impl DLL loopback
// rq_post_iss_object_v2 impl DLL rpclib
