#ifndef CRADLE_INNER_SERVICE_REQUEST_H
#define CRADLE_INNER_SERVICE_REQUEST_H

#include <stdexcept>
#include <type_traits>

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/caching/immutable.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/caching/secondary_cache_intf.h>
#include <cradle/inner/caching/secondary_cache_serialization.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/remote.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/secondary_cached_blob.h>

/*
 * Service resolving a request to a value
 *
 * The public interface is resolve_request().
 * There also is resolve_request_local() intended to be called by the
 * framework.
 *
 * (Almost) all functions have a Context template argument. It is assumed that
 * a caller knows the actual context class, and so that type is also passed
 * around by the functions in this file.
 *
 * Context implementation classes should be declared "final" whenever possible
 * to profit from the "if constexpr" constructs: these prevent compilation
 * errors on paths not taken, and diminish object code size.
 */

namespace cradle {

template<Context Ctx, Request Req>
cppcoro::shared_task<typename Req::value_type>
resolve_request_uncached(Ctx& ctx, Req const& req)
{
    // TODO maybe let req.resolve() return shared_task
    co_return co_await req.resolve(ctx);
}

// Resolves a memory-cached request using some sort of secondary cache.
// A memory-cached request needs no secondary cache, so it can be resolved
// right away (by calling the request's function).
template<Context Ctx, MemoryCachedRequest Req>
cppcoro::task<typename Req::value_type>
resolve_secondary_cached(Ctx& ctx, Req const& req)
{
    return req.resolve(ctx);
}

// Resolves a fully-cached request using some sort of secondary cache, and some
// sort of serialization.
template<Context Ctx, FullyCachedRequest Req>
cppcoro::task<typename Req::value_type>
resolve_secondary_cached(Ctx& ctx, Req const& req)
{
    using Value = typename Req::value_type;
    inner_resources& resources{ctx.get_resources()};
    captured_id const& key{req.get_captured_id()};
    auto create_blob_task = [&]() -> cppcoro::task<blob> {
        co_return serialize_secondary_cache_value(co_await req.resolve(ctx));
    };
    co_return deserialize_secondary_cache_value<Value>(
        co_await secondary_cached_blob(resources, key, create_blob_task));
}

// This function, being a coroutine, takes key by value.
// The caller should ensure that cache, ctx and req outlive the coroutine.
template<Context Ctx, CachedRequest Req>
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
        auto value = co_await resolve_secondary_cached(ctx, req);
        record_immutable_cache_value(cache, *key, deep_sizeof(value));
        co_return value;
    }
    catch (...)
    {
        record_immutable_cache_failure(cache, *key);
        throw;
    }
}

template<Context Ctx, CachedIntrospectiveRequest Req>
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
template<Context Ctx, CachedNonIntrospectiveRequest Req>
cppcoro::shared_task<typename Req::value_type> const&
resolve_request_cached_sync(Ctx& ctx, Req const& req)
{
    // Must not be called for runtime-remote contexts
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
template<Context Ctx, CachedIntrospectiveRequest Req>
cppcoro::shared_task<typename Req::value_type>
resolve_request_cached_sync(Ctx& ctx, Req const& req)
{
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

template<Context Ctx, Request Req>
cppcoro::shared_task<typename Req::value_type>
resolve_request_cached_async(Ctx& ctx, Req const& req)
{
    auto task = resolve_request_cached_sync(ctx, req);
    auto result = co_await task;
    auto* actx = to_local_async_context_intf(ctx);
    // If function ran, status already will be FINISHED
    // If result came from cache, it will not yet be
    actx->update_status(async_status::FINISHED);
    co_return result;
}

template<Context Ctx, CachedRequest Req>
cppcoro::shared_task<typename Req::value_type>
resolve_request_cached(Ctx& ctx, Req const& req)
{
    if constexpr (DefinitelyAsyncContext<Ctx>)
    {
        return resolve_request_cached_async(ctx, req);
    }
    else if constexpr (DefinitelySyncContext<Ctx>)
    {
        return resolve_request_cached_sync(ctx, req);
    }
    else
    {
        if (ctx.is_async())
        {
            return resolve_request_cached_async(ctx, req);
        }
        else
        {
            return resolve_request_cached_sync(ctx, req);
        }
    }
}

template<Request Req>
cppcoro::shared_task<typename Req::value_type>
resolve_request_remote(remote_context_intf& ctx, Req const& req)
{
    co_return resolve_remote_to_value(ctx, req);
}

// Framework-internal interface: resolve_request_local()
// Anything above should be called only from within this file.

template<Context Ctx, typename Val>
    requires(!Request<Val>)
cppcoro::task<Val> resolve_request_local(Ctx& ctx, Val const& val)
{
    // async status, if appropriate, should already be FINISHED
    co_return val;
}

template<Context Ctx, Request Req>
cppcoro::shared_task<typename Req::value_type>
resolve_request_local(Ctx& ctx, Req const& req)
{
    static_assert(UncachedRequest<Req> || CachingContext<Ctx>);
    if constexpr (UncachedRequest<Req>)
    {
        return resolve_request_uncached(ctx, req);
    }
    else
    {
        return resolve_request_cached(ctx, req);
    }
}

// Public interface: resolve_request()

// The "requires" here is strictly not needed: if it is omitted, and the second
// arg is a request, then the following template would subsume.
// However, when that template is not selected for some
// reason, the compiler will emit unclear error messages.
template<Context Ctx, typename Val>
    requires(!Request<Val>)
cppcoro::task<Val> resolve_request(Ctx& ctx, Val const& val)
{
    // async status, if appropriate, should already be FINISHED
    co_return val;
}

/*
 * Resolve a request, remotely or locally, depending on context
 *
 * This function is blocking. Progress of an asynchronous
 * request can be monitored via its context tree.
 * This function throws async_cancelled when an asynchronous request is
 * cancelled.
 *
 * - shared_task because that's what resolve_request_cached_sync() returns
 * - It seems likely that for multiple calls for the same Request,
 *   Context will be the same in each case (so just one template
 *   instantiation)
 */
template<Context Ctx, Request Req>
cppcoro::shared_task<typename Req::value_type>
resolve_request(Ctx& ctx, Req const& req)
{
    // Assume that the actual context class is known when calling this function
    static_assert(!std::is_abstract_v<Ctx>);
    // Context implementation classes should be declared "final"
    static_assert(std::is_final_v<Ctx>);
    static_assert(ValidContext<Ctx>);
    if constexpr (DefinitelyRemoteContext<Ctx>)
    {
        // cppcoro::task return value also possible
        return resolve_request_remote(ctx, req);
    }
    else if constexpr (DefinitelyLocalContext<Ctx>)
    {
        return resolve_request_local(ctx, req);
    }
    else
    {
        if (auto* rctx = to_remote_context_intf(ctx))
        {
            return resolve_request_remote(*rctx, req);
        }
        else
        {
            return resolve_request_local(ctx, req);
        }
    }
}

} // namespace cradle

#endif
