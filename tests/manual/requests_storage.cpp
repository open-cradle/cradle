#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>

#include "../support/inner_service.h"
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/encodings/msgpack_value.h>
#include <cradle/inner/resolve/seri_req.h>
#include <cradle/inner/resolve/seri_result.h>
#include <cradle/inner/service/request_store.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/testing_seri_catalog.h>
#include <cradle/plugins/requests_storage/http/http_requests_storage.h>

/*
 * Manual tests to demonstrate storing requests on secondary (local) and
 * tertiary (remote) storage.
 *
 * The tests rely on a bazel-remote server, which must be started first:
 *   $ export STORAGE_DIR=/path/to/requests-storage
 *   $ mkdir -p ${STORAGE_DIR}
 *   $ docker run -u 1000:1000 \
 *    -v ${STORAGE_DIR}:/data \
 *    -v $HOME/.aws:/aws-config \
 *    -p 9092:8080 buchgr/bazel-remote-cache \
 *    --max_size=1000 \
 *    --disable_http_ac_validation=1 \
 *    --s3.auth_method=aws_credentials_file \
 *    --s3.aws_shared_credentials_file=/aws-config/credentials \
 *    --s3.endpoint=s3.eu-central-1.amazonaws.com \
 *    --s3.bucket=user-s3-requests
 * filling in STORAGE_DIR and --s3.bucket with appropriate values.
 *
 * Then:
 * - First store a request:
 *     $ ./tests/manual_test_runner "[manual][requests_store][store]"
 * - Check that the request has indeed been added on the storage medium.
 * - Load the request from the storage and verify it:
 *     $ ./tests/manual_test_runner "[manual][requests_store][load]"
 *
 * For convenience, the storage re-uses the bazel-remote solution already
 * implemented for caching request resolution results. Request keys are
 * identical between the two caches, so care must be taken that there is no
 * overlap, meaning the following arguments should differ:
 * - STORAGE_DIR
 * - Port number (9092 versus 9090)
 * - S3 bucket
 * Requests storage should be permanent. This is achieved by using a cache
 * with a "really big" limit (the --max_size argument), but it won't be the
 * most reliable solution.
 */

using namespace cradle;

static char const tag_store[] = "[manual][requests_store][store]";
static char const tag_load[] = "[manual][requests_store][load]";

// SHA-256 hash over the request used in these tests
static std::string const the_key
    = "9a292f6cbb9ce61ba4612a5f115fa48829e617d7f3d5187e938cb959f7f5cf9d";

TEST_CASE("store request", tag_store)
{
    inner_resources resources{make_inner_tests_config()};
    auto owned_storage{std::make_unique<http_requests_storage>(resources)};
    resources.set_requests_storage(std::move(owned_storage));
    testing_seri_catalog cat{resources.get_seri_registry()};

    auto req0{rq_make_some_blob<caching_level_type::full>(5, false)};
    cppcoro::sync_wait(store_request(req0, resources));

    REQUIRE(get_request_key(req0) == the_key);
}

TEST_CASE("load stored request", tag_load)
{
    inner_resources resources{make_inner_tests_config()};
    auto owned_storage{std::make_unique<http_requests_storage>(resources)};
    resources.set_requests_storage(std::move(owned_storage));
    testing_seri_catalog cat{resources.get_seri_registry()};

    auto req_written{rq_make_some_blob<caching_level_type::full>(5, false)};
    using Req = decltype(req_written);

    std::string key{the_key};
    auto req_read = cppcoro::sync_wait(load_request<Req>(key, resources));
    REQUIRE(req_read == req_written);
}

TEST_CASE("load and resolve stored request", tag_load)
{
    auto owned_resources{make_inner_test_resources()};
    auto& resources{*owned_resources};
    auto owned_storage{std::make_unique<http_requests_storage>(resources)};
    auto& storage{*owned_storage};
    resources.set_requests_storage(std::move(owned_storage));
    testing_seri_catalog cat{resources.get_seri_registry()};

    std::string key{the_key};
    auto opt_req_blob{cppcoro::sync_wait(storage.read(key))};
    if (!opt_req_blob)
    {
        throw not_found_error(
            fmt::format("Storage has no entry with key {}", key));
    }
    std::string req_serialized{to_string(*opt_req_blob)};

    testing_request_context ctx{resources, ""};
    serialized_result seri_result{
        cppcoro::sync_wait(resolve_serialized_local(ctx, req_serialized))};
    blob result = deserialize_value<blob>(seri_result.value());

    // expected[i+1] == 3 * expected[i] + 1
    std::byte expected[]
        = {std::byte{0},
           std::byte{1},
           std::byte{4},
           std::byte{13},
           std::byte{40}};
    REQUIRE(result == make_static_blob(expected, sizeof(expected)));
}
