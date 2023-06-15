#include <deque>

#include <cradle/inner/remote/config.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/remote/wait_async.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/remote.h>

namespace cradle {

namespace {

// Matches if the remote operation has finished successfully
class async_finished_matcher : public async_status_matcher
{
 public:
    async_finished_matcher(spdlog::logger& logger);

    bool operator()(async_status) const override;
};

async_finished_matcher::async_finished_matcher(spdlog::logger& logger)
    : async_status_matcher{"async_finished_matcher", logger}
{
}

bool
async_finished_matcher::operator()(async_status status) const
{
    bool done = status == async_status::FINISHED;
    report_status(status, done);
    return done;
}

void
wait_until_async_finished(remote_proxy& proxy, async_id remote_id)
{
    auto& logger = proxy.get_logger();
    wait_until_async_status_matches(
        proxy, remote_id, async_finished_matcher(logger));
}

serialized_result
resolve_async(remote_async_context_intf& ctx, std::string seri_req)
{
    remote_proxy* proxy{};
    async_id remote_id{};
    try
    {
        proxy = &find_proxy(ctx.proxy_name());
        auto& logger = proxy->get_logger();
        logger.debug(
            "resolve_async on {}: {} ...",
            ctx.domain_name(),
            seri_req.substr(0, 10));
        remote_id
            = proxy->submit_async(ctx.make_config(), std::move(seri_req));
        ctx.set_remote_id(remote_id);
    }
    catch (...)
    {
        ctx.fail_remote_id();
        throw;
    }
    wait_until_async_finished(*proxy, remote_id);
    return proxy->get_async_response(remote_id);
}

serialized_result
resolve_sync(remote_context_intf& ctx, std::string seri_req)
{
    auto& proxy = find_proxy(ctx.proxy_name());
    auto& logger = proxy.get_logger();
    logger.debug(
        "request on {}: {} ...", ctx.domain_name(), seri_req.substr(0, 10));
    return proxy.resolve_sync(ctx.make_config(), std::move(seri_req));
}

} // namespace

serialized_result
resolve_remote(remote_context_intf& ctx, std::string seri_req)
{
    if (auto* async_ctx = cast_ctx_to_ptr<remote_async_context_intf>(ctx))
    {
        return resolve_async(*async_ctx, std::move(seri_req));
    }
    else
    {
        return resolve_sync(ctx, std::move(seri_req));
    }
}

} // namespace cradle
