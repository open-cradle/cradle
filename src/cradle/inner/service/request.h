#ifndef CRADLE_INNER_SERVICE_REQUEST_H
#define CRADLE_INNER_SERVICE_REQUEST_H

#include <optional>

#include <cppcoro/fmap.hpp>
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
cppcoro::task<typename Request::value_type>
resolve_request(Context const& ctx, Request const& req)
{
    if constexpr (Request::caching_level == caching_level_type::none)
    {
        return req.resolve(ctx);
    }
    else
    {
        return resolve_request_cached(ctx, req);
    }
}

template<typename Context, typename Request>
cppcoro::task<typename Request::value_type>
resolve_request(Context const& ctx, std::unique_ptr<Request> const& req)
{
    if constexpr (Request::caching_level == caching_level_type::none)
    {
        return req->resolve(ctx);
    }
    else
    {
        return resolve_request_cached(ctx, *req);
    }
}

template<typename Context, typename Request>
cppcoro::task<typename Request::value_type>
resolve_request(Context const& ctx, std::shared_ptr<Request> const& req)
{
    if constexpr (Request::caching_level == caching_level_type::none)
    {
        return req->resolve(ctx);
    }
    else
    {
        return resolve_request_cached(ctx, *req);
    }
}

} // namespace cradle

#endif
