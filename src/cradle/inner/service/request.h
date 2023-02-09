#ifndef CRADLE_INNER_SERVICE_REQUEST_H
#define CRADLE_INNER_SERVICE_REQUEST_H

#include <stdexcept>
#include <type_traits>

#include <cppcoro/fmap.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/caching/immutable.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/disk_cache_intf.h>
#include <cradle/inner/service/disk_cache_serialization.h>
#include <cradle/inner/service/disk_cached_blob.h>
#include <cradle/inner/service/remote.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

// Resolves a memory-cached request using some sort of disk cache.
// A memory-cached request needs no disk cache, so it can be resolved
// right away (by calling the request's function).
template<typename Ctx, MemoryCachedRequest Req>
requires ContextMatchingRequest<Ctx, Req>
    cppcoro::task<typename Req::value_type>
    resolve_disk_cached(Ctx& ctx, Req const& req)
{
    return req.resolve(ctx);
}

// Resolves a fully-cached request using some sort of disk cache, and some
// sort of serialization.
template<typename Ctx, FullyCachedRequest Req>
requires ContextMatchingRequest<Ctx, Req>
    cppcoro::task<typename Req::value_type>
    resolve_disk_cached(Ctx& ctx, Req const& req)
{
    using Value = typename Req::value_type;
    inner_resources& resources{ctx.get_resources()};
    captured_id const& key{req.get_captured_id()};
    auto create_blob_task = [&]() -> cppcoro::task<blob> {
        co_return serialize_disk_cache_value(co_await req.resolve(ctx));
    };
    co_return deserialize_disk_cache_value<Value>(
        co_await disk_cached_blob(resources, key, create_blob_task));
}

// This function, being a coroutine, takes key by value.
// The caller should ensure that cache, ctx and req outlive the coroutine.
template<typename Ctx, CachedRequest Req>
requires ContextMatchingRequest<Ctx, Req>
    cppcoro::shared_task<typename Req::value_type>
    resolve_request_on_memory_cache_miss(
        detail::immutable_cache_impl& cache,
        captured_id key,
        Ctx& ctx,
        Req const& req)
{
    // cache and key could be retrieved from ctx and req, respectively.
    try
    {
        auto value = co_await resolve_disk_cached(ctx, req);
        record_immutable_cache_value(cache, *key, deep_sizeof(value));
        co_return value;
    }
    catch (...)
    {
        record_immutable_cache_failure(cache, *key);
        throw;
    }
}

template<typename Ctx, CachedIntrospectiveRequest Req>
requires ContextMatchingRequest<Ctx, Req>
    cppcoro::shared_task<typename Req::value_type>
    resolve_request_introspective(
        Ctx& ctx,
        Req const& req,
        cppcoro::shared_task<typename Req::value_type> shared_task,
        tasklet_tracker& client)
{
    client.on_before_await(
        req.get_introspection_title(), *req.get_captured_id());
    auto res = co_await shared_task;
    client.on_after_await();
    co_return res;
}

// Returns a reference to the shared_task stored in the memory cache.
// Not copying shared_task's looks like a worthwhile optimization.
template<typename Ctx, CachedNonIntrospectiveRequest Req>
requires ContextMatchingRequest<Ctx, Req>
    cppcoro::shared_task<typename Req::value_type> const&
    resolve_request_cached(Ctx& ctx, Req const& req)
{
    if (ctx.remotely())
    {
        // This optimization rules out the combination of a run-time remote
        // context and a build-time non-introspective request.
        throw std::logic_error("TODO cannot remote here");
    }
    immutable_cache_ptr<typename Req::value_type> ptr{
        ctx.get_cache(),
        req.get_captured_id(),
        [&](detail::immutable_cache_impl& internal_cache,
            captured_id const& key) {
            return resolve_request_on_memory_cache_miss<Ctx, Req>(
                internal_cache, key, ctx, req);
        }};
    return ptr.task();
}

// Returns the shared_task by value.
template<typename Ctx, CachedIntrospectiveRequest Req>
requires ContextMatchingRequest<Ctx, Req>
    cppcoro::shared_task<typename Req::value_type>
    resolve_request_cached(Ctx& ctx, Req const& req)
{
    if (remote_context_intf* rem_ctx = to_remote_context_intf(ctx))
    {
        return resolve_remote_to_value(*rem_ctx, req);
    }
    immutable_cache_ptr<typename Req::value_type> ptr{
        ctx.get_cache(),
        req.get_captured_id(),
        [&](detail::immutable_cache_impl& internal_cache,
            captured_id const& key) {
            return resolve_request_on_memory_cache_miss<Ctx, Req>(
                internal_cache, key, ctx, req);
        }};
    auto shared_task = ptr.task();
    if (auto tasklet = ctx.get_tasklet())
    {
        return resolve_request_introspective<Ctx, Req>(
            ctx, req, shared_task, *tasklet);
    }
    return shared_task;
}

// If second arg is a request, one of the following templates will subsume.
// (Maybe that arg should be Val&& but then the mechanism doesn't work.)
template<typename Ctx, typename Val>
cppcoro::task<Val>
resolve_request(Ctx& ctx, Val const& val)
{
    co_return val;
}

template<typename Ctx, UncachedRequest Req>
requires ContextMatchingRequest<Ctx, Req> auto
resolve_request(Ctx& ctx, Req const& req)
{
    return req.resolve(ctx);
}

template<typename Ctx, CachedNonIntrospectiveRequest Req>
requires ContextMatchingRequest<Ctx, Req>
    cppcoro::shared_task<typename Req::value_type> const&
    resolve_request(Ctx& ctx, Req const& req)
{
    return resolve_request_cached(ctx, req);
}

template<typename Ctx, CachedIntrospectiveRequest Req>
requires ContextMatchingRequest<Ctx, Req>
    cppcoro::shared_task<typename Req::value_type>
    resolve_request(Ctx& ctx, Req const& req)
{
    return resolve_request_cached(ctx, req);
}

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
