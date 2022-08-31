#include <fstream>
#include <type_traits>

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cppcoro/sync_wait.hpp>
#include <spdlog/spdlog.h>

#include "../../inner/introspection/tasklet_testing.h"
#include "../../inner/support/concurrency_testing.h"
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/service/request.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/typing/encodings/msgpack.h>
#include <cradle/typing/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

template<typename Request>
static void
test_serialize_thinknode_request(
    Request const& req,
    auto& deserialize_request,
    auto& validate_request,
    std::string const& filename)
{
    // Validate original request
    validate_request(req);

    // Serialize the original request
    {
        std::ofstream ofs(filename);
        cereal::JSONOutputArchive oarchive(ofs);
        // Not
        //   oarchive(req);
        // which adds an undesired outer element.
        req.save(oarchive);
    }

    // Deserialize and verify the resulting request equals the original
    std::ifstream ifs(filename);
    cereal::JSONInputArchive iarchive(ifs);
    Request req1{deserialize_request(iarchive)};
    REQUIRE(req1.hash() == req.hash());
    REQUIRE(
        get_unique_string(*req1.get_captured_id())
        == get_unique_string(*req.get_captured_id()));
    if constexpr (Request::introspective)
    {
        REQUIRE(
            req1.get_introspection_title() == req.get_introspection_title());
    }
    validate_request(req1);
}

// Make a "post ISS object" request, where the payload is a blob
template<caching_level_type Level>
auto
make_post_iss_request_constant()
{
    return rq_post_iss_object<Level>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"));
}

// Make a "post ISS object" request, where the payload is a dynamic
template<caching_level_type Level>
auto
make_post_iss_request_dynamic()
{
    return rq_post_iss_object<Level>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        dynamic("payload"));
}

// Make a "post ISS object" request, where the payload comes from a
// type-erased request
template<caching_level_type Level>
auto
make_post_iss_request_request()
{
    std::ostringstream uuid;
    uuid << "uuid_payload_" << static_cast<int>(Level);
    auto make_blob_function
        = [](std::string const& payload) { return make_blob(payload); };
    auto make_blob_request = rq_function_erased_uuid<Level>(
        uuid.str(), make_blob_function, rq_value(std::string("payload")));
    return rq_post_iss_object<Level>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob_request);
}

// Make a type-erased "post ISS object" request, where the payload is a blob
template<caching_level_type Level>
auto
make_post_iss_request_erased_constant()
{
    return rq_post_iss_object_erased<Level>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob("payload"));
}

// Make a type-erased "post ISS object" request, where the payload comes
// from another type-erased request
template<caching_level_type Level>
auto
make_post_iss_request_erased_request()
{
    auto make_blob_function
        = [](std::string const& payload) { return make_blob(payload); };
    auto make_blob_request = rq_function_erased_uuid<Level>(
        "uuid_100", make_blob_function, rq_value(std::string("payload")));
    return rq_post_iss_object_erased<Level>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob_request);
}

template<typename Request>
static void
test_post_iss_request(
    Request const& req, bool introspected, bool use_dynamic = false)
{
    constexpr caching_level_type level = Request::caching_level;
    clean_tasklet_admin_fixture fixture;
    service_core service;
    init_test_service(service);

    auto& mock_http = enable_http_mocking(service);
    auto payload = use_dynamic ? value_to_msgpack_blob(dynamic("payload"))
                               : make_blob("payload");
    mock_http.set_script(
        {{make_http_request(
              http_request_method::POST,
              "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"},
               {"Content-Type", "application/octet-stream"}},
              payload),
          make_http_200_response("{ \"id\": \"def\" }")}});

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    tasklet_tracker* tasklet = nullptr;
    if (introspected)
    {
        tasklet = create_tasklet_tracker("my_pool", "my_title");
    }
    thinknode_request_context ctx{service, session, tasklet};

    // Resolve using task, storing result in configured caches
    auto id0 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(id0 == "def");
    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    if (introspected)
    {
        auto infos = get_tasklet_infos(true);
        // my_post_iss_object_request, HTTP request
        REQUIRE(infos.size() == 2);
        REQUIRE(infos[0].pool_name() == "my_pool");
        REQUIRE(infos[0].title() == "my_title");
        // scheduled, before_co_await, ...
        REQUIRE(infos[0].events().size() >= 2);
        auto const& event01 = infos[0].events()[1];
        REQUIRE(event01.what() == tasklet_event_type::BEFORE_CO_AWAIT);
        REQUIRE(event01.details().starts_with("my_post_iss_object_request"));
        REQUIRE(infos[1].pool_name() == "HTTP");
        REQUIRE(infos[1].title() == "HTTP: post https://mgh.thinknode.io/api/v1.0/iss/string?context=123");
    }

    if constexpr (level >= caching_level_type::memory)
    {
        // Resolve using memory cache
        auto id1 = cppcoro::sync_wait(resolve_request(ctx, req));
        REQUIRE(id1 == "def");
    }

    if constexpr (level >= caching_level_type::full)
    {
        sync_wait_write_disk_cache(service);
        service.inner_reset_memory_cache(
            immutable_cache_config{0x40'00'00'00});

        // Resolve using disk cache
        auto id2 = cppcoro::sync_wait(resolve_request(ctx, req));
        REQUIRE(id2 == "def");
    }
}

TEST_CASE("ISS POST - blob, uncached", "[requests]")
{
    test_post_iss_request(
        make_post_iss_request_constant<caching_level_type::none>(), false);
}

TEST_CASE("ISS POST - blob, memory cached", "[requests]")
{
    test_post_iss_request(
        make_post_iss_request_request<caching_level_type::memory>(), true);
}

TEST_CASE("ISS POST - blob, fully cached", "[requests]")
{
    test_post_iss_request(
        make_post_iss_request_request<caching_level_type::full>(), false);
}

TEST_CASE("ISS POST - dynamic, uncached", "[requests]")
{
    test_post_iss_request(
        make_post_iss_request_dynamic<caching_level_type::none>(),
        false,
        true);
}

static void
test_serialize_post_iss(auto const& req, std::string const& filename)
{
    // For the type-erased requests, req_type is
    // thinknode_request_erased<caching_level_type::full, std::string>.
    // For the container-based requests, it's something horrible.
    using req_type = std::remove_cvref_t<decltype(req)>;
    auto deserialize = [](cereal::JSONInputArchive& iarchive) {
        return req_type(iarchive);
    };
    auto test_request
        = [](auto const& req1) { test_post_iss_request(req1, false); };
    test_serialize_thinknode_request(req, deserialize, test_request, filename);
}

TEST_CASE("ISS POST serialization - container, blob", "[cereal]")
{
    auto req{make_post_iss_request_request<caching_level_type::full>()};
    test_serialize_post_iss(req, "iss_post_container_blob.json");
}

TEST_CASE("ISS POST serialization - erased, blob", "[cereal]")
{
    auto req{
        make_post_iss_request_erased_constant<caching_level_type::full>()};
    test_serialize_post_iss(req, "iss_post_erased_blob.json");
}

TEST_CASE("ISS POST serialization - erased, inner request", "[cereal]")
{
    auto req{make_post_iss_request_erased_request<caching_level_type::full>()};
    test_serialize_post_iss(req, "iss_post_erased_inner_request.json");
}

template<typename Request>
static void
test_retrieve_immutable_object_req(Request const& req) requires(
    Request::caching_level == caching_level_type::full)
{
    service_core service;
    init_test_service(service);
    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";
    auto tasklet = create_tasklet_tracker("my_pool", "my_title");
    thinknode_request_context ctx{service, session, tasklet};
    auto expected{make_blob(std::string("payload"))};
    auto& mock_http = enable_http_mocking(service);
    mock_http.set_script(
        {{make_get_request(
              "https://mgh.thinknode.io/api/v1.0/iss/immutable/"
              "abc?context=123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/octet-stream"}}),
          make_http_200_response("payload")}});

    // Resolve using HTTP, storing result in both caches
    auto blob0 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(blob0 == expected);
    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());

    // Resolve using memory cache
    auto blob1 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(blob1 == expected);

    sync_wait_write_disk_cache(service);
    service.inner_reset_memory_cache(immutable_cache_config{0x40'00'00'00});

    // Resolve using disk cache
    auto blob2 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(blob2 == expected);
}

static void
test_retrieve_immutable_object(auto create_req)
{
    string api_url{"https://mgh.thinknode.io/api/v1.0"};
    string context_id{"123"};
    string immutable_id{"abc"};
    auto req{create_req(api_url, context_id, immutable_id)};
    test_retrieve_immutable_object_req(req);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - class, fully cached", "[requests]")
{
    // Using my_retrieve_immutable_object_request_base and mixin
    test_retrieve_immutable_object(
        rq_retrieve_immutable_object<caching_level_type::full>);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - function, fully cached", "[requests]")
{
    // Using function_request_erased
    test_retrieve_immutable_object(
        rq_retrieve_immutable_object_func<caching_level_type::full>);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT serialization - class", "[cereal]")
{
    auto req{rq_retrieve_immutable_object<caching_level_type::full>(
        "https://mgh.thinknode.io/api/v1.0", "123", "abc")};
    auto deserialize = [](cereal::JSONInputArchive& iarchive) {
        return thinknode_request_erased<caching_level_type::full, blob>(
            iarchive);
    };
    test_serialize_thinknode_request(
        req,
        deserialize,
        test_retrieve_immutable_object_req<decltype(req)>,
        "retrieve_immutable_class.json");
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT serialization - function", "[cereal]")
{
    auto req{rq_retrieve_immutable_object_func<caching_level_type::full>(
        "https://mgh.thinknode.io/api/v1.0", "123", "abc")};
    auto deserialize = [](cereal::JSONInputArchive& iarchive) {
        return function_request_erased<
            caching_level_type::full,
            blob,
            true,
            true>(iarchive);
    };
    test_serialize_thinknode_request(
        req,
        deserialize,
        test_retrieve_immutable_object_req<decltype(req)>,
        "retrieve_immutable_func.json");
}
