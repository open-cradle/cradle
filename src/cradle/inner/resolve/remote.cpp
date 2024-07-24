#include <deque>

#include <fmt/format.h>

#include <cradle/inner/remote/config.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/remote/wait_async.h>
#include <cradle/inner/requests/cast_ctx.h>
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

void
set_lock_ptr_record(
    cache_record_lock* lock_ptr,
    remote_proxy& proxy,
    serialized_result const& seri_resp)
{
    if (lock_ptr)
    {
        auto record_id{seri_resp.get_cache_record_id()};
        if (!record_id.is_set())
        {
            // This is a protocol error.
            throw remote_error{"remote did not set record_id"};
        }
        lock_ptr->set_record(
            std::make_unique<remote_locked_cache_record>(proxy, record_id));
    }
}

serialized_result
resolve_async(
    remote_async_context_intf& ctx,
    std::string seri_req,
    cache_record_lock* lock_ptr)
{
    remote_proxy* proxy{};
    async_id remote_id{};
    try
    {
        proxy = &ctx.get_proxy();
        auto& logger = proxy->get_logger();
        logger.debug(
            "resolve_async on {}: {} ...",
            ctx.domain_name(),
            seri_req.substr(0, 10));
        bool need_record_lock{lock_ptr != nullptr};
        remote_id = proxy->submit_async(
            ctx.make_config(need_record_lock), std::move(seri_req));
        ctx.set_remote_id(remote_id);
        if (ctx.introspective())
        {
            fmt::print("submit_async: remote_id {}\n", remote_id);
        }
    }
    catch (...)
    {
        ctx.fail_remote_id();
        throw;
    }
    wait_until_async_finished(*proxy, remote_id);
    auto seri_resp = proxy->get_async_response(remote_id);
    set_lock_ptr_record(lock_ptr, *proxy, seri_resp);
    return seri_resp;
}

serialized_result
resolve_sync(
    remote_context_intf& ctx,
    std::string seri_req,
    cache_record_lock* lock_ptr)
{
    auto& proxy = ctx.get_proxy();
    auto& logger = proxy.get_logger();
    logger.debug(
        "request on {}: {} ...", ctx.domain_name(), seri_req.substr(0, 10));
    bool need_record_lock{lock_ptr != nullptr};
    auto seri_resp = proxy.resolve_sync(
        ctx.make_config(need_record_lock), std::move(seri_req));
    set_lock_ptr_record(lock_ptr, proxy, seri_resp);
    return seri_resp;
}

} // namespace

remote_locked_cache_record::remote_locked_cache_record(
    remote_proxy& proxy, remote_cache_record_id record_id)
    : proxy_{proxy}, record_id_{record_id}
{
}

remote_locked_cache_record::~remote_locked_cache_record()
{
    proxy_.release_cache_record_lock(record_id_);
}

serialized_result
resolve_remote(
    remote_context_intf& ctx,
    std::string seri_req,
    cache_record_lock* lock_ptr)
{
    if (auto* async_ctx = cast_ctx_to_ptr<remote_async_context_intf>(ctx))
    {
        return resolve_async(*async_ctx, std::move(seri_req), lock_ptr);
    }
    else
    {
        return resolve_sync(ctx, std::move(seri_req), lock_ptr);
    }
}

} // namespace cradle
