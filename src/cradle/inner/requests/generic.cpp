#include <cassert>

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

} // namespace cradle
