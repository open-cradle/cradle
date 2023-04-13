#include <chrono>
#include <deque>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/remote.h>

namespace cradle {

namespace {

class async_status_matcher
{
 public:
    virtual ~async_status_matcher() = default;

    virtual bool operator()(async_status) const = 0;
};

void
wait_until_async_done(
    remote_proxy& proxy,
    async_id remote_id,
    std::unique_ptr<async_status_matcher> matcher)
{
    for (;;)
    {
        auto status = proxy.get_async_status(remote_id);
        if ((*matcher)(status))
        {
            break;
        }
        else if (status == async_status::CANCELLED)
        {
            throw async_cancelled(
                fmt::format("remote async {} cancelled", remote_id));
        }
        else if (status == async_status::ERROR)
        {
            std::string errmsg = proxy.get_async_error_message(remote_id);
            throw async_error(errmsg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void
report_matcher_status(
    spdlog::logger& logger,
    std::string const& matcher_name,
    async_status status,
    bool done)
{
    if (!done)
    {
        logger.debug("{}: status {}, NOT done", matcher_name, status);
    }
    else
    {
        logger.debug("{}: status {}, DONE", matcher_name, status);
    }
}

class subs_available_matcher : public async_status_matcher
{
 public:
    subs_available_matcher(spdlog::logger& logger);

    bool operator()(async_status) const override;

 private:
    spdlog::logger& logger_;
};

subs_available_matcher::subs_available_matcher(spdlog::logger& logger)
    : logger_{logger}
{
}

bool
subs_available_matcher::operator()(async_status status) const
{
    bool done = status == async_status::SUBS_RUNNING
                || status == async_status::SELF_RUNNING
                || status == async_status::FINISHED;
    report_matcher_status(logger_, "subs_available_matcher", status, done);
    return done;
}

void
wait_until_subs_available(remote_proxy& proxy, async_id remote_id)
{
    auto& logger = proxy.get_logger();
    wait_until_async_done(
        proxy, remote_id, std::make_unique<subs_available_matcher>(logger));
}

class async_finished_matcher : public async_status_matcher
{
 public:
    async_finished_matcher(spdlog::logger& logger);

    bool operator()(async_status) const override;

 private:
    spdlog::logger& logger_;
};

async_finished_matcher::async_finished_matcher(spdlog::logger& logger)
    : logger_{logger}
{
}

bool
async_finished_matcher::operator()(async_status status) const
{
    bool done = status == async_status::FINISHED;
    report_matcher_status(logger_, "async_finished_matcher", status, done);
    return done;
}

void
wait_until_async_finished(remote_proxy& proxy, async_id remote_id)
{
    auto& logger = proxy.get_logger();
    wait_until_async_done(
        proxy, remote_id, std::make_unique<async_finished_matcher>(logger));
}

void
populate_remote_ctx_tree(
    remote_proxy& proxy, remote_async_context_intf& root_ctx)
{
    std::deque<remote_async_context_intf*> todo_list;
    todo_list.push_back(&root_ctx);
    while (!todo_list.empty())
    {
        auto* todo_ctx = todo_list.front();
        todo_list.pop_front();
        auto child_specs = proxy.get_sub_contexts(todo_ctx->get_remote_id());
        for (auto& spec : child_specs)
        {
            auto [sub_aid, is_req] = spec;
            auto& sub_ctx = todo_ctx->add_sub(sub_aid, is_req);
            todo_list.push_back(&sub_ctx);
        }
    }
}

serialized_result
resolve_async(
    remote_proxy& proxy,
    remote_async_context_intf& root_ctx,
    std::string domain_name,
    std::string seri_req)
{
    auto& logger = proxy.get_logger();
    logger.debug("resolve_async");
    auto remote_id = proxy.submit_async(
        root_ctx, std::move(domain_name), std::move(seri_req));
    root_ctx.set_remote_id(remote_id);
    if (root_ctx.cancellation_pending())
    {
        proxy.request_cancellation(remote_id);
        throw remote_error{"cancelled"};
    }
    wait_until_subs_available(proxy, remote_id);
    populate_remote_ctx_tree(proxy, root_ctx);
    wait_until_async_finished(proxy, remote_id);
    return proxy.get_async_response(remote_id);
}

serialized_result
resolve_sync(
    remote_context_intf& ctx,
    remote_proxy& proxy,
    std::string domain_name,
    std::string seri_req)
{
    return proxy.resolve_sync(
        ctx, std::move(domain_name), std::move(seri_req));
}

} // namespace

serialized_result
resolve_remote(remote_context_intf& ctx, std::string seri_req)
{
    auto& proxy = find_proxy(ctx.proxy_name());
    auto& logger = proxy.get_logger();
    std::string domain_name{ctx.domain_name()};
    logger.debug("request on {}: {} ...", domain_name, seri_req.substr(0, 10));
    if (auto* async_ctx = to_remote_async_context_intf(ctx))
    {
        return resolve_async(
            proxy, *async_ctx, std::move(domain_name), std::move(seri_req));
    }
    else
    {
        return resolve_sync(
            ctx, proxy, std::move(domain_name), std::move(seri_req));
    }
}

} // namespace cradle
