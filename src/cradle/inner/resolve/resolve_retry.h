#ifndef CRADLE_INNER_RESOLVE_RESOLVE_RETRY_H
#define CRADLE_INNER_RESOLVE_RESOLVE_RETRY_H

#include <cradle/inner/resolve/resolve_request.h>

namespace cradle {

template<typename Orig>
using ConstraintsForRetry = ResolutionConstraints<
    Orig::force_remote,
    Orig::force_local,
    Orig::force_sync,
    Orig::force_async,
    true>;

template<
    Context Ctx,
    RetryableRequest Req,
    typename Constraints = DefaultResolutionConstraints<Ctx>>
cppcoro::task<typename Req::value_type>
resolve_request_with_retry(
    Ctx& ctx,
    Req const& req,
    Constraints constraints = Constraints(),
    cache_record_lock* lock_ptr = nullptr)
{
    static_assert(ValidRetryableRequest<Req>);
    int attempt = 0;
    auto resolve_task = resolve_request(ctx, req, lock_ptr, constraints);
    for (;;)
    {
        std::string what;
        try
        {
            co_return co_await resolve_task;
        }
        catch (std::exception const& e)
        {
            what = e.what();
        }
        // if introspective: update status. Not really possible with
        // tasklet_tracker.
        auto interval{req.prepare_retry(attempt, what)};
        // schedule_after() cannot be cancelled as for current sync
        auto& io_svc{ctx.get_resources().the_io_service()};
        co_await io_svc.schedule_after(interval);
        resolve_task = resolve_request(
            ctx, req, lock_ptr, ConstraintsForRetry<Constraints>());
        ++attempt;
    }
    throw std::logic_error("impossible");
}

} // namespace cradle

#endif
