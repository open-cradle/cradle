#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/requests/generic.h>

namespace cradle {

context_tasklet::context_tasklet(
    introspected_context_intf& ctx,
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

context_tasklet::~context_tasklet()
{
    if (ctx_)
    {
        ctx_->pop_tasklet();
    }
}

} // namespace cradle
