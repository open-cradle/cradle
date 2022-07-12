#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/thinknode/utilities.h>

namespace cradle {

my_post_iss_object_request_base::my_post_iss_object_request_base(
    std::string api_url,
    std::string context_id,
    thinknode_type_info schema,
    blob object_data)
    : api_url_{api_url},
      context_id_{std::move(context_id)},
      url_type_string_{get_url_type_string(api_url, schema)},
      object_data_{std::move(object_data)}
{
}

cppcoro::task<string>
my_post_iss_object_request_base::resolve(
    thinknode_request_context const& ctx) const
{
    auto query = make_http_request(
        http_request_method::POST,
        api_url_ + "/iss/" + url_type_string_ + "?context=" + context_id_,
        {{"Authorization", "Bearer " + ctx.session.access_token},
         {"Accept", "application/json"},
         {"Content-Type", "application/octet-stream"}},
        object_data_);

    auto response = co_await async_http_request(ctx, std::move(query));

    co_return from_dynamic<id_response>(parse_json_response(response)).id;
}

} // namespace cradle
