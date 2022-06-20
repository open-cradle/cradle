#ifndef CRADLE_INNER_SERVICE_REQUEST_H
#define CRADLE_INNER_SERVICE_REQUEST_H

#include <type_traits>

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/caching/disk_cache.h>
#include <cradle/inner/caching/immutable.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/internals.h>
#include <cradle/inner/service/types.h>

namespace cradle {

template<typename Ctx, typename Req>
requires IntrospectiveContextRequest<Ctx, Req>
    cppcoro::shared_task<typename Req::value_type>
    resolve_request_introspected(
        Ctx& ctx,
        Req const& req,
        cppcoro::shared_task<typename Req::value_type> shared_task,
        tasklet_tracker& client)
{
    client.on_before_await(req.get_summary(), *req.get_captured_id());
    auto res = co_await shared_task;
    client.on_after_await();
    co_return res;
}

template<typename Ctx, typename Req>
requires CachedContextRequest<Ctx, Req>
    cppcoro::shared_task<typename Req::value_type>
    resolve_request_cached(Ctx& ctx, Req const& req)
{
    immutable_cache_ptr<typename Req::value_type> ptr{ctx, req};
    auto shared_task = ptr.task();
    if constexpr (Req::introspective)
    {
        if (ctx.tasklet)
        {
            return resolve_request_introspected<Ctx, Req>(
                ctx, req, shared_task, *ctx.tasklet);
        }
    }
    return shared_task;
}

template<typename Ctx, typename Req>
requires UncachedContextRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, Req const& req)
{
    return req.resolve(ctx);
}

template<typename Ctx, typename Req>
requires CachedContextRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, Req const& req)
{
    return resolve_request_cached(ctx, req);
}

template<typename Ctx, typename Req>
requires UncachedContextRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, std::unique_ptr<Req> const& req)
{
    return req->resolve(ctx);
}

template<typename Ctx, typename Req>
requires CachedContextRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, std::unique_ptr<Req> const& req)
{
    return resolve_request_cached(ctx, *req);
}

template<typename Ctx, typename Req>
requires UncachedContextRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, std::shared_ptr<Req> const& req)
{
    return req->resolve(ctx);
}

template<typename Ctx, typename Req>
requires CachedContextRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, std::shared_ptr<Req> const& req)
{
    return resolve_request_cached(ctx, *req);
}

} // namespace cradle

#endif
