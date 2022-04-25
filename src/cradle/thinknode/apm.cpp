#include <cradle/thinknode/apm.h>

#include <cradle/thinknode/utilities.h>
#include <cradle/typing/core/monitoring.h>
#include <cradle/typing/encodings/sha256_hash_id.h>
#include <cradle/typing/io/http_requests.hpp>

namespace cradle {

namespace uncached {

cppcoro::task<thinknode_app_version_info>
get_app_version_info(
    thinknode_request_context ctx, string account, string app, string version)
{
    auto query = make_get_request(
        ctx.session.api_url + "/apm/apps/" + account + "/" + app + "/versions/"
            + version + "?include_manifest=true",
        {{"Authorization", "Bearer " + ctx.session.access_token},
         {"Accept", "application/json"}});

    auto response = co_await async_http_request(ctx, query);
    co_return from_dynamic<thinknode_app_version_info>(
        parse_json_response(response));
}

} // namespace uncached

cppcoro::shared_task<thinknode_app_version_info>
get_app_version_info(
    thinknode_request_context ctx, string account, string app, string version)
{
    string function_name{"get_app_version_info"};
    auto cache_key = make_captured_sha256_hashed_id(
        function_name, ctx.session.api_url, account, app, version);
    auto create_task = [=]() {
        return uncached::get_app_version_info(ctx, account, app, version);
    };
    return make_shared_task_for_cacheable<thinknode_app_version_info>(
        ctx.service,
        cache_key,
        create_task,
        ctx.tasklet,
        std::move(function_name));
}

} // namespace cradle
