#include <cassert>

#include <fmt/format.h>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/requests/generic.h>

namespace cradle {

tasklet_context::tasklet_context(
    introspective_context_intf& ctx,
    std::string const& pool_name,
    std::string const& title)
{
    auto tasklet = create_tasklet_tracker(pool_name, title, ctx.get_tasklet());
    if (tasklet)
    {
        ctx_ = &ctx;
        ctx_->push_tasklet(tasklet);
    }
}

tasklet_context::~tasklet_context()
{
    if (ctx_)
    {
        ctx_->pop_tasklet();
    }
}

remote_context_intf*
to_remote_context_intf(context_intf& ctx)
{
    if (!ctx.remotely())
    {
        return nullptr;
    }
    remote_context_intf* rem_ctx = dynamic_cast<remote_context_intf*>(&ctx);
    assert(rem_ctx != nullptr);
    return rem_ctx;
}

local_async_context_intf*
to_local_async_context_intf(context_intf& ctx)
{
    return dynamic_cast<local_async_context_intf*>(&ctx);
}

std::shared_ptr<local_async_context_intf>
to_local_async_context_intf(std::shared_ptr<context_intf> const& ctx)
{
    return std::dynamic_pointer_cast<local_async_context_intf>(ctx);
}

remote_async_context_intf*
to_remote_async_context_intf(context_intf& ctx)
{
    return dynamic_cast<remote_async_context_intf*>(&ctx);
}

std::string
to_string(async_status s)
{
    char const* res = nullptr;
    switch (s)
    {
        case async_status::CREATED:
            res = "CREATED";
            break;
        case async_status::SUBS_RUNNING:
            res = "SUBS_RUNNING";
            break;
        case async_status::SELF_RUNNING:
            res = "SELF_RUNNING";
            break;
        case async_status::CANCELLING:
            res = "CANCELLING";
            break;
        case async_status::CANCELLED:
            res = "CANCELLED";
            break;
        case async_status::FINISHED:
            res = "FINISHED";
            break;
        case async_status::ERROR:
            res = "ERROR";
            break;
        default:
            return fmt::format("bad async_status {}", static_cast<int>(s));
    }
    return std::string{res};
}

} // namespace cradle
