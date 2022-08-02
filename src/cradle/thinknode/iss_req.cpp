#include <cradle/thinknode/iss_req.h>
#include <cradle/thinknode/utilities.h>

namespace cradle {

cppcoro::task<std::string>
resolve_my_post_iss_object_request(
    thinknode_request_context& ctx,
    std::string const& api_url,
    std::string const& context_id,
    std::string const& url_type_string,
    blob const& object_data)
{
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
resolve_my_retrieve_immutable_object_request(
    thinknode_request_context& ctx,
    std::string const& api_url,
    std::string const& context_id,
    std::string const& immutable_id)
{
    auto query = make_get_request(
        api_url + "/iss/immutable/" + immutable_id + "?context=" + context_id,
        {{"Authorization", "Bearer " + ctx.session.access_token},
         {"Accept", "application/octet-stream"}});
    auto response = co_await async_http_request(ctx, std::move(query));
    co_return response.body;
}

} // namespace cradle
