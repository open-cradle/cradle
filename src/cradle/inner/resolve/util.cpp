#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/requests/cast_ctx.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/seri_req.h>
#include <cradle/inner/resolve/util.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

coawait_introspection::coawait_introspection(
    introspective_context_intf& ctx,
    std::string const& pool_name,
    std::string const& title)
    : ctx_{ctx},
      tasklet_{create_tasklet_tracker(
          ctx.get_resources().the_tasklet_admin(),
          pool_name,
          title,
          ctx.get_tasklet())}
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

cppcoro::task<serialized_result>
resolve_serialized_introspective(
    introspective_context_intf& ctx,
    std::string proxy_name,
    std::string title,
    std::string seri_req,
    seri_cache_record_lock_t seri_lock)
{
    // Ensure that the tasklet's first timestamp coincides (almost) with the
    // "co_await shared_task".
    co_await dummy_coroutine();
    coawait_introspection guard{ctx, proxy_name, title};
    auto& loc_ctx{cast_ctx_to_ref<local_context_intf>(ctx)};
    co_return co_await resolve_serialized_local(
        loc_ctx, std::move(seri_req), std::move(seri_lock));
}

} // namespace cradle
