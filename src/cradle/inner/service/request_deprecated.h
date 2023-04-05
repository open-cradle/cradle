#ifndef CRADLE_INNER_SERVICE_REQUEST_DEPRECATED_H
#define CRADLE_INNER_SERVICE_REQUEST_DEPRECATED_H

#include <memory>

#include <cradle/inner/requests/generic.h>

namespace cradle {

// resolve_request() variants needed only for the deprecated functions
// in function_deprecated.h
template<typename Ctx, UncachedRequest Req>
requires ContextMatchingRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, std::unique_ptr<Req> const& req)
{
    return req->resolve(ctx);
}

template<typename Ctx, CachedRequest Req>
requires ContextMatchingRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, std::unique_ptr<Req> const& req)
{
    return resolve_request_cached(ctx, *req);
}

template<typename Ctx, UncachedRequest Req>
requires ContextMatchingRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, std::shared_ptr<Req> const& req)
{
    return req->resolve(ctx);
}

template<typename Ctx, CachedRequest Req>
requires ContextMatchingRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, std::shared_ptr<Req> const& req)
{
    return resolve_request_cached(ctx, *req);
}

} // namespace cradle

#endif
