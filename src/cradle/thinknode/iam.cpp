#include <cradle/thinknode/iam.h>

#include <cradle/inner/core/sha256_hash_id.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/thinknode/disk_cache_serialization.h>
#include <cradle/thinknode/utilities.h>
#include <cradle/typing/core/monitoring.h>
#include <cradle/typing/io/http_requests.hpp>

namespace cradle {

namespace uncached {

cppcoro::task<thinknode_context_contents>
get_context_contents(thinknode_request_context ctx, string context_id)
{
    auto query = make_get_request(
        ctx.session.api_url + "/iam/contexts/" + context_id,
        {{"Authorization", "Bearer " + ctx.session.access_token},
         {"Accept", "application/json"}});

    auto response = co_await async_http_request(ctx, query);

    co_return from_dynamic<thinknode_context_contents>(
        parse_json_response(response));
}

} // namespace uncached

cppcoro::task<string>
get_context_id(thinknode_request_context ctx, string realm)
{
    auto query = make_get_request(
        ctx.session.api_url + "/iam/realms/" + realm + "/context",
        {{"Authorization", "Bearer " + ctx.session.access_token},
         {"Accept", "application/json"}});

    auto response = co_await async_http_request(ctx, query);

    co_return from_dynamic<id_response>(parse_json_response(response)).id;
}

cppcoro::shared_task<thinknode_context_contents>
get_context_contents(thinknode_request_context ctx, string context_id)
{
    string function_name{"get_context_contents"};
    auto cache_key = make_captured_sha256_hashed_id(
        function_name, ctx.session.api_url, context_id);
    auto create_task
        = [=]() { return uncached::get_context_contents(ctx, context_id); };
    return make_shared_task_for_cacheable<thinknode_context_contents>(
        ctx.service,
        cache_key,
        create_task,
        ctx.get_tasklet(),
        std::move(function_name));
}

cppcoro::shared_task<string>
get_context_bucket(thinknode_request_context ctx, string context_id)
{
    auto context = co_await get_context_contents(ctx, std::move(context_id));
    co_return context.bucket;
}

} // namespace cradle
