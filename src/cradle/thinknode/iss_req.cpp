#include <cassert>

#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/thinknode/utilities.h>

namespace cradle {

cppcoro::task<std::string>
resolve_my_post_iss_object_request(
    cached_introspected_context_intf& ctx_intf,
    std::string const& api_url,
    std::string const& context_id,
    std::string const& url_type_string,
    blob const& object_data)
{
    assert(dynamic_cast<thinknode_request_context*>(&ctx_intf));
    auto ctx{static_cast<thinknode_request_context&>(ctx_intf)};
    auto query = make_http_request(
        http_request_method::POST,
        api_url + "/iss/" + url_type_string + "?context=" + context_id,
        {{"Authorization", "Bearer " + ctx.session.access_token},
         {"Accept", "application/json"},
         {"Content-Type", "application/octet-stream"}},
        object_data);

    auto response = co_await async_http_request(ctx, std::move(query));

    co_return from_dynamic<id_response>(parse_json_response(response)).id;
}

cppcoro::task<blob>
retrieve_immutable_blob_uncached_erased(
    cached_introspected_context_intf& ctx,
    std::string api_url,
    std::string context_id,
    std::string immutable_id)
{
    assert(dynamic_cast<thinknode_request_context*>(&ctx));
    return retrieve_immutable_blob_uncached(
        static_cast<thinknode_request_context&>(ctx),
        std::move(context_id),
        std::move(immutable_id));
}

} // namespace cradle
