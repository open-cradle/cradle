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

template<typename Context, typename Request>
cppcoro::shared_task<typename Request::value_type>
resolve_request_cached(Context& ctx, Request const& req)
{
    static_assert(Request::caching_level == caching_level_type::memory);
    using Value = typename Request::value_type;
    inner_service_core& svc{ctx.service};
    immutable_cache_ptr<Value> ptr(svc.inner_internals().cache, ctx, req);
    return ptr.task();
}

template<typename Context, typename Request>
std::enable_if_t<
    Request::caching_level == caching_level_type::none,
    cppcoro::task<typename Request::value_type>>
resolve_request(Context& ctx, Request const& req)
{
    return req.resolve(ctx);
}

template<typename Context, typename Request>
std::enable_if_t<
    Request::caching_level != caching_level_type::none,
    cppcoro::shared_task<typename Request::value_type>>
resolve_request(Context& ctx, Request const& req)
{
    return resolve_request_cached(ctx, req);
}

template<typename Context, typename Request>
std::enable_if_t<
    Request::caching_level == caching_level_type::none,
    cppcoro::task<typename Request::value_type>>
resolve_request(Context& ctx, std::unique_ptr<Request> const& req)
{
    return req->resolve(ctx);
}

template<typename Context, typename Request>
std::enable_if_t<
    Request::caching_level != caching_level_type::none,
    cppcoro::shared_task<typename Request::value_type>>
resolve_request(Context& ctx, std::unique_ptr<Request> const& req)
{
    return resolve_request_cached(ctx, *req);
}

template<typename Context, typename Request>
std::enable_if_t<
    Request::caching_level == caching_level_type::none,
    cppcoro::task<typename Request::value_type>>
resolve_request(Context& ctx, std::shared_ptr<Request> const& req)
{
    return req->resolve(ctx);
}

template<typename Context, typename Request>
std::enable_if_t<
    Request::caching_level != caching_level_type::none,
    cppcoro::shared_task<typename Request::value_type>>
resolve_request(Context& ctx, std::shared_ptr<Request> const& req)
{
    return resolve_request_cached(ctx, *req);
}

} // namespace cradle

#endif
