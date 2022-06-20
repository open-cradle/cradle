#include <cradle/thinknode/iss_req.h>

#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/utilities.h>
#include <cradle/typing/encodings/msgpack.h>

namespace cradle {

my_post_iss_object_request::my_post_iss_object_request(
    string context_id, thinknode_type_info schema, blob object_data)
    : context_id_{std::move(context_id)},
      schema_{std::move(schema)},
      object_data_{std::move(object_data)}
{
}

cppcoro::task<string>
my_post_iss_object_request::resolve(thinknode_request_context const& ctx) const
{
    auto query = make_http_request(
        http_request_method::POST,
        ctx.session.api_url + "/iss/"
            + get_url_type_string(ctx.session, schema_)
            + "?context=" + context_id_,
        {{"Authorization", "Bearer " + ctx.session.access_token},
         {"Accept", "application/json"},
         {"Content-Type", "application/octet-stream"}},
        object_data_);

    auto response = co_await async_http_request(ctx, std::move(query));

    co_return from_dynamic<id_response>(parse_json_response(response)).id;
}

my_post_iss_object_request
rq_post_iss_object(
    string context_id, thinknode_type_info schema, blob object_data)
{
    return my_post_iss_object_request{
        std::move(context_id), std::move(schema), std::move(object_data)};
}

my_post_iss_object_request
rq_post_iss_object(string context_id, thinknode_type_info schema, dynamic data)
{
    blob msgpack_data = value_to_msgpack_blob(data);
    return my_post_iss_object_request{
        std::move(context_id), std::move(schema), std::move(msgpack_data)};
}

} // namespace cradle
