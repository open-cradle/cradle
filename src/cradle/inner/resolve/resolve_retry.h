#ifndef CRADLE_INNER_RESOLVE_RESOLVE_REQUEST_RETRY_H
#define CRADLE_INNER_RESOLVE_RESOLVE_REQUEST_RETRY_H

#include <cradle/inner/resolve/resolve_request.h>

namespace cradle {

// TODO abusing is_sub to mean "do not populate"
template<typename Orig>
using ConstraintsWithSub = ResolutionConstraints<
    Orig::force_remote,
    Orig::force_local,
    Orig::force_sync,
    Orig::force_async,
    true>;

template<
    Context Ctx,
    Request Req,
    typename Constraints = DefaultResolutionConstraints<Ctx>>
cppcoro::task<typename Req::value_type>
resolve_request_with_retry(
    Ctx& ctx,
    Req const& req,
    Constraints constraints = Constraints(),
    cache_record_lock* lock_ptr = nullptr)
{
    // TODO move logging to get_retry_interval()
    auto logger{ensure_logger("TRK")};
    logger->info("resolve_request_with_retry");
    int attempt = 0;
    auto resolve_task = resolve_request(ctx, req, lock_ptr, constraints);
    for (;;)
    {
        std::string what;
        try
        {
            logger->info("co_await resolve_request attempt {}", attempt);
            co_return co_await resolve_task;
        }
        catch (std::exception const& e)
        {
            what = e.what();
        }
        // if introspective: update status. Not really possible with
        // tasklet_tracker.
        logger->error("caught exc {}", what);
        // this should come from request's props
        // std::chrono::duration interval{req.get_retry_interval(attempt,
        // what)};
        auto interval{std::chrono::milliseconds{100}};
        // schedule_after() cannot be cancelled as for current sync
        auto& io_svc{ctx.get_resources().the_io_service()};
        co_await io_svc.schedule_after(interval);
        resolve_task = resolve_request(
            ctx, req, lock_ptr, ConstraintsWithSub<Constraints>());
        ++attempt;
    }
    throw std::logic_error("impossible");
}

} // namespace cradle

#endif
