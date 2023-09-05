#include <cradle/inner/resolve/util.h>

namespace cradle {

coawait_introspection::coawait_introspection(
    introspective_context_intf& ctx,
    std::string const& pool_name,
    std::string const& title)
    : ctx_{ctx},
      tasklet_{create_tasklet_tracker(pool_name, title, ctx.get_tasklet())}
{
    if (tasklet_)
    {
        ctx_.push_tasklet(*tasklet_);
        tasklet_->on_running();
    }
}

coawait_introspection::~coawait_introspection()
{
    if (tasklet_)
    {
        tasklet_->on_finished();
        ctx_.pop_tasklet();
    }
}

} // namespace cradle
