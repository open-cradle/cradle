#include <algorithm>
#include <fstream>
#include <iterator>
#include <type_traits>

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cppcoro/sync_wait.hpp>
#include <spdlog/spdlog.h>

#include "../../inner/introspection/tasklet_testing.h"
#include "../../inner/support/concurrency_testing.h"
#include "../../inner/support/request.h"
#include "iss_req_common.h"
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/service/request.h>
#include <cradle/thinknode/iss_req_class.h>
#include <cradle/typing/io/mock_http.h>
#include <cradle/typing/service/core.h>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

template<typename Request>
auto
deserialize_thinknode(cereal::JSONInputArchive& iarchive)
{
    constexpr auto caching_level = Request::caching_level;
    using value_type = typename Request::value_type;
    thinknode_request_erased<caching_level, value_type> req;
    req.load(iarchive);
    return req;
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
// sub-request
template<caching_level_type Level>
auto
make_post_iss_request_subreq(std::string payload = "payload")
{
    std::ostringstream uuid;
    uuid << "uuid_" << payload << "_" << static_cast<int>(Level);
    auto make_blob_function
        = [](std::string const& payload) { return make_blob(payload); };
    request_props<Level> props{request_uuid(uuid.str())};
    auto make_blob_request
        = rq_function_erased(props, make_blob_function, rq_value(payload));
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
// from a sub-request
template<caching_level_type Level>
auto
make_post_iss_request_erased_subreq()
{
    auto make_blob_function
        = [](std::string const& payload) { return make_blob(payload); };
    request_props<Level> props{request_uuid("uuid_100")};
    auto make_blob_request = rq_function_erased(
        props, make_blob_function, rq_value(std::string("payload")));
    return rq_post_iss_object_erased<Level>(
        "https://mgh.thinknode.io/api/v1.0",
        "123",
        make_thinknode_type_info_with_string_type(thinknode_string_type()),
        make_blob_request);
}

static void
test_serialize_post_iss(auto const& req, std::string const& filename)
{
    // For the type-erased requests, req_type is
    // thinknode_request_erased<caching_level_type::full, std::string>.
    // For the container-based requests, it's something horrible.
    using req_type = std::remove_cvref_t<decltype(req)>;
    auto deserialize = [](cereal::JSONInputArchive& iarchive) {
        req_type req1;
        req1.load(iarchive);
        return req1;
    };
    auto test_request
        = [](auto const& req1) { test_post_iss_request(req1, false); };
    test_serialize_thinknode_request(req, deserialize, test_request, filename);
}

TEST_CASE("ISS POST - blob, uncached", "[iss_req_class]")
{
    test_post_iss_request(
        make_post_iss_request_constant<caching_level_type::none>(), false);
}

TEST_CASE("ISS POST - blob, memory cached", "[iss_req_class]")
{
    test_post_iss_request(
        make_post_iss_request_subreq<caching_level_type::memory>(), true);
}

TEST_CASE("ISS POST - blob, fully cached", "[iss_req_class]")
{
    test_post_iss_request(
        make_post_iss_request_subreq<caching_level_type::full>(), false);
}

TEST_CASE("ISS POST - dynamic, uncached", "[iss_req_class]")
{
    test_post_iss_request(
        make_post_iss_request_dynamic<caching_level_type::none>(),
        false,
        true);
}

TEST_CASE("ISS POST - fully cached, parallel", "[iss_req_class]")
{
    constexpr caching_level_type level = caching_level_type::full;
    using Req = decltype(make_post_iss_request_subreq<level>());
    std::vector<blob> payloads;
    std::vector<Req> requests;
    std::vector<std::string> results;
    for (int i = 0; i < 7; ++i)
    {
        // 7 requests / results, but only
        // 3 unique payloads / requests / results
        int req_id = i % 3;
        std::ostringstream ss_payload;
        ss_payload << "payload_" << req_id;
        auto payload = ss_payload.str();
        if (req_id == i)
        {
            payloads.emplace_back(make_blob(payload));
        }

        requests.emplace_back(make_post_iss_request_subreq<level>(payload));

        std::ostringstream ss_result;
        ss_result << "result_" << req_id;
        auto result = ss_result.str();
        results.emplace_back(result);
    }

    test_post_iss_requests_parallel(requests, payloads, results);
}

TEST_CASE("ISS POST serialization - container, blob", "[iss_req_class]")
{
    auto req{make_post_iss_request_subreq<caching_level_type::full>()};
    test_serialize_post_iss(req, "iss_post_container_blob.json");
}

TEST_CASE("ISS POST serialization - erased, blob", "[iss_req_class]")
{
    auto req{
        make_post_iss_request_erased_constant<caching_level_type::full>()};
    test_serialize_post_iss(req, "iss_post_erased_blob.json");
}

TEST_CASE("ISS POST serialization - erased, inner request", "[iss_req_class]")
{
    auto req{make_post_iss_request_erased_subreq<caching_level_type::full>()};
    test_serialize_post_iss(req, "iss_post_erased_inner_request.json");
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - class, fully cached", "[iss_req_class]")
{
    test_retrieve_immutable_object(
        rq_retrieve_immutable_object<caching_level_type::full>, "abc");
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT - fully cached, parallel", "[iss_req_class]")
{
    constexpr caching_level_type level = caching_level_type::full;
    using Req = thinknode_request_erased<level, blob>;
    std::vector<std::string> object_ids;
    std::vector<Req> requests;
    std::vector<std::string> responses;
    for (int i = 0; i < 7; ++i)
    {
        int req_id = i % 3;
        std::ostringstream ss_object_id;
        ss_object_id << "abc" << req_id;
        auto object_id = ss_object_id.str();
        if (req_id == i)
        {
            object_ids.push_back(object_id);
        }

        requests.push_back(rq_retrieve_immutable_object<level>(
            "https://mgh.thinknode.io/api/v1.0", "123", object_id));

        std::ostringstream ss_response;
        ss_response << "payload_" << req_id;
        auto response = ss_response.str();
        responses.push_back(response);
    }
    test_retrieve_immutable_object_parallel(requests, object_ids, responses);
}

TEST_CASE("RETRIEVE IMMUTABLE OBJECT serialization - class", "[iss_req_class]")
{
    auto req{rq_retrieve_immutable_object<caching_level_type::full>(
        "https://mgh.thinknode.io/api/v1.0", "123", "abc")};
    test_serialize_thinknode_request(
        req,
        deserialize_thinknode<decltype(req)>,
        test_retrieve_immutable_object_req<decltype(req)>,
        "retrieve_immutable_class.json");
}
