#include <chrono>
#include <deque>

#include <cppcoro/schedule_on.hpp>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/service/remote.h>

namespace cradle {

namespace {

class async_status_matcher
{
 public:
    virtual ~async_status_matcher() = default;

    virtual bool operator()(async_status) const = 0;
};

cppcoro::task<void>
wait_until_async_done(
    remote_proxy& proxy,
    async_id remote_id,
    std::unique_ptr<async_status_matcher> matcher)
{
    for (;;)
    {
        auto status = co_await proxy.get_async_status(remote_id);
        if ((*matcher)(status))
        {
            break;
        }
        else if (status == async_status::CANCELLED)
        {
            throw remote_error("remote async cancelled");
        }
        else if (status == async_status::ERROR)
        {
            // TODO get error reason
            throw remote_error("remote async failed");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    co_return;
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

cppcoro::task<void>
wait_until_subs_available(remote_proxy& proxy, async_id remote_id)
{
    auto& logger = proxy.get_logger();
    co_await wait_until_async_done(
        proxy, remote_id, std::make_unique<subs_available_matcher>(logger));
    co_return;
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

cppcoro::task<void>
wait_until_async_finished(remote_proxy& proxy, async_id remote_id)
{
    auto& logger = proxy.get_logger();
    return wait_until_async_done(
        proxy, remote_id, std::make_unique<async_finished_matcher>(logger));
}

cppcoro::task<void>
populate_remote_ctx_tree(
    remote_proxy& proxy, remote_async_context_intf& root_ctx)
{
    std::deque<remote_async_context_intf*> todo_list;
    todo_list.push_back(&root_ctx);
    while (!todo_list.empty())
    {
        auto* todo_ctx = todo_list.front();
        todo_list.pop_front();
        auto child_specs
            = co_await proxy.get_sub_contexts(todo_ctx->get_remote_id());
        for (auto& spec : child_specs)
        {
            auto [sub_aid, is_req] = spec;
            auto& sub_ctx = todo_ctx->add_sub(sub_aid, is_req);
            todo_list.push_back(&sub_ctx);
        }
    }
    co_return;
}

cppcoro::task<serialized_result>
resolve_async(
    remote_proxy& proxy,
    remote_async_context_intf& ctx,
    std::string domain_name,
    std::string seri_req)
{
    auto& logger = proxy.get_logger();
    logger.debug("resolve_async");
    co_await proxy.get_coro_thread_pool().schedule();
    auto remote_id = co_await proxy.submit_async(
        std::move(domain_name), std::move(seri_req));
    ctx.set_remote_id(remote_id);
    co_await wait_until_subs_available(proxy, remote_id);
    co_await populate_remote_ctx_tree(proxy, ctx);
    co_await wait_until_async_finished(proxy, remote_id);
    co_return co_await proxy.get_async_response(remote_id);
}

cppcoro::task<serialized_result>
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

cppcoro::task<serialized_result>
resolve_remote(remote_context_intf& ctx, std::string seri_req)
{
    auto& proxy = find_proxy(ctx.proxy_name());
    auto& logger = proxy.get_logger();
    std::string domain_name{ctx.domain_name()};
    logger.debug("request on {}: {} ...", domain_name, seri_req.substr(0, 10));
    cppcoro::task<serialized_result> task;
    if (auto* async_ctx = to_remote_async_context_intf(ctx))
    {
        task = resolve_async(
            proxy, *async_ctx, std::move(domain_name), std::move(seri_req));
    }
    else
    {
        task = resolve_sync(
            ctx, proxy, std::move(domain_name), std::move(seri_req));
    }
    return task;
}

} // namespace cradle
