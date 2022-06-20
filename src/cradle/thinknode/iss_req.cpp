#include <cradle/thinknode/iss_req.h>

#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/utilities.h>

namespace cradle {

my_post_iss_object_request_base::my_post_iss_object_request_base(
    string context_id, thinknode_type_info schema, blob object_data)
    : context_id_{std::move(context_id)},
      schema_{std::move(schema)},
      object_data_{std::move(object_data)}
{
    // This is where the constructor spends most of its time.
    // The id is needed only when caching, so the current code does not
    // let us measure how long it really takes to create an uncached
    // ISS POST request object.
    create_id();
}

void
my_post_iss_object_request_base::create_id()
{
    // TODO captured_id for my_post_iss_object_request
    id_ = make_captured_id(context_id_);
}

cppcoro::task<string>
my_post_iss_object_request_base::resolve(
    thinknode_request_context const& ctx) const
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

} // namespace cradle
