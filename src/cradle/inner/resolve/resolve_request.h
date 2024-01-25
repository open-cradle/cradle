#ifndef CRADLE_INNER_RESOLVE_RESOLVE_REQUEST_H
#define CRADLE_INNER_RESOLVE_RESOLVE_REQUEST_H

#include <stdexcept>
#include <type_traits>

#include <cppcoro/task.hpp>

#include <cradle/inner/caching/immutable.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/caching/secondary_cache_serialization.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/remote.h>
#include <cradle/inner/resolve/util.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/secondary_cached_blob.h>
#include <cradle/inner/service/secondary_storage_intf.h>

/*
 * Service resolving a request to a value
 *
 * The public interface is resolve_request().
 */

namespace cradle {

// Constrain the options for resolve_request(). Useful because:
// (a) Code for a non-selected option might not compile
// (b) The generated object code size will be lower
// (c) The actual context class may not be known at the resolve_request()
//     call location (e.g. because the context object was created via the
//     domain interface class)
// (d) The actual context class might implement options that the caller is
//     not interested in (e.g., it's in a local-only environment)
template<
    bool ForceRemote = false,
    bool ForceLocal = false,
    bool ForceSync = false,
    bool ForceAsync = false>
struct ResolutionConstraints
{
    static_assert(!(ForceRemote && ForceLocal));
    static_assert(!(ForceSync && ForceAsync));

    static constexpr bool force_remote = ForceRemote;
    static constexpr bool force_local = ForceLocal;
    static constexpr bool force_sync = ForceSync;
    static constexpr bool force_async = ForceAsync;

    ResolutionConstraints()
    {
    }
};

using NoResolutionConstraints
    = ResolutionConstraints<false, false, false, false>;
using ResolutionConstraintsLocal
    = ResolutionConstraints<false, true, false, false>;
using ResolutionConstraintsLocalSync
    = ResolutionConstraints<false, true, true, false>;
using ResolutionConstraintsLocalAsync
    = ResolutionConstraints<false, true, false, true>;
using ResolutionConstraintsRemoteSync
    = ResolutionConstraints<true, false, true, false>;
using ResolutionConstraintsRemoteAsync
    = ResolutionConstraints<true, false, false, true>;

// These defaults should make it superfluous for the caller to specify the
// constraints, if the actual context class is final and known at the
// resolve_request() call location.
template<Context Ctx>
using DefaultResolutionConstraints = ResolutionConstraints<
    DefinitelyRemoteContext<Ctx>,
    DefinitelyLocalContext<Ctx>,
    DefinitelySyncContext<Ctx>,
    DefinitelyAsyncContext<Ctx>>;

// Holds for std::true_type and std::false_type only
template<typename T>
concept BoolConst
    = requires {
          requires std::same_as<std::remove_const_t<decltype(T::value)>, bool>;
      };

template<Context Ctx, Request Req, BoolConst Async>
cppcoro::task<typename Req::value_type>
resolve_request_uncached(Ctx& ctx, Req const& req, Async async)
{
    // TODO maybe let req.resolve_*sync() return cppcoro::task
    if constexpr (Async::value)
    {
        auto& actx = cast_ctx_to_ref<local_async_context_intf>(ctx);
        co_return co_await req.resolve_async(actx);
    }
    else
    {
        auto& lctx = cast_ctx_to_ref<local_context_intf>(ctx);
        co_return co_await req.resolve_sync(lctx);
    }
}

// Resolves a memory-cached request using some sort of secondary cache.
// A memory-cached request needs no secondary cache, so it can be resolved
// right away (by calling the request's function).
template<Context Ctx, MemoryCachedRequest Req, BoolConst Async>
cppcoro::task<typename Req::value_type>
resolve_secondary_cached(Ctx& ctx, Req const& req, Async async)
{
    if constexpr (Async::value)
    {
        auto& actx = cast_ctx_to_ref<local_async_context_intf>(ctx);
        return req.resolve_async(actx);
    }
    else
    {
        auto& lctx = cast_ctx_to_ref<local_context_intf>(ctx);
        return req.resolve_sync(lctx);
    }
}

// Resolves a fully-cached request using some sort of secondary cache, and some
// sort of serialization.
template<Context Ctx, FullyCachedRequest Req, BoolConst Async>
cppcoro::task<typename Req::value_type>
resolve_secondary_cached(Ctx& ctx, Req const& req, Async async)
{
    using Value = typename Req::value_type;
    auto& cac_ctx = cast_ctx_to_ref<caching_context_intf>(ctx);
    inner_resources& resources{cac_ctx.get_resources()};
    auto create_blob_task = [&]() -> cppcoro::task<blob> {
        if constexpr (Async::value)
        {
            auto& actx = cast_ctx_to_ref<local_async_context_intf>(ctx);
            co_return serialize_secondary_cache_value(
                co_await req.resolve_async(actx));
        }
        else
        {
            auto& lctx = cast_ctx_to_ref<local_context_intf>(ctx);
            co_return serialize_secondary_cache_value(
                co_await req.resolve_sync(lctx));
        }
    };
    co_return deserialize_secondary_cache_value<Value>(
        co_await secondary_cached_blob(
            resources, req.get_captured_id(), std::move(create_blob_task)));
}

// Called if the action cache contains no record for this request.
// Resolves the request, stores the result in the CAS, updates the action
// cache. The cache is accessed via ptr. The caller should ensure that ctx, req
// and ptr outlive the coroutine.
template<Context Ctx, CachedRequest Req, BoolConst Async>
cppcoro::shared_task<void>
resolve_request_on_memory_cache_miss(
    Ctx& ctx,
    Req const& req,
    Async async,
    immutable_cache_ptr<typename Req::value_type>& ptr)
{
    try
    {
        ptr.record_value(co_await resolve_secondary_cached(ctx, req, async));
    }
    catch (...)
    {
        ptr.record_failure();
        throw;
    }
}

template<Context Ctx, CachedRequest Req, BoolConst Async>
cppcoro::task<typename Req::value_type>
resolve_request_cached(Ctx& ctx, Req const& req, Async async)
{
    using value_type = typename Req::value_type;
    using ptr_type = immutable_cache_ptr<value_type>;
    auto& cac_ctx = cast_ctx_to_ref<caching_context_intf>(ctx);
    // While ptr lives, the corresponding cache record lives too.
    // ptr lives until the shared_task has run (on behalf of the current
    // request, or a previous one), and the value has been retrieved from the
    // cache record.
    ptr_type ptr{
        cac_ctx.get_resources().memory_cache(),
        req.get_captured_id(),
        [&](untyped_immutable_cache_ptr& ptr) {
            return resolve_request_on_memory_cache_miss(
                ctx, req, async, static_cast<ptr_type&>(ptr));
        }};
    if constexpr (IntrospectiveRequest<Req>)
    {
        auto& intr_ctx = cast_ctx_to_ref<introspective_context_intf>(ctx);
        // Have a dedicated tasklet track the co_await on ptr's shared_task.
        // Ensure that the tasklet's first timestamp coincides (almost) with
        // the "co_await shared_task".
        co_await dummy_coroutine();
        coawait_introspection guard{
            intr_ctx, "resolve_request", req.get_introspection_title()};
        // co_await' ptr's shared_task, ensuring that its value is available.
        co_await ptr.ensure_value_task();
    }
    else
    {
        // co_await' ptr's shared_task, ensuring that its value is available.
        co_await ptr.ensure_value_task();
    }
    // Finally, return the shared_task's value.
    co_return ptr.get_value();
}

template<Context Ctx, CachedRequest Req>
cppcoro::task<typename Req::value_type>
resolve_request_async_cached(Ctx& ctx, Req const& req)
{
    auto result = co_await resolve_request_cached(ctx, req, std::true_type{});
    // If function ran, status already will be FINISHED
    // If result came from cache, it will not yet be
    auto& actx = cast_ctx_to_ref<local_async_context_intf>(ctx);
    actx.update_status(async_status::FINISHED);
    co_return result;
}

template<Context Ctx, Request Req>
cppcoro::task<typename Req::value_type>
resolve_request_sync(Ctx& ctx, Req const& req)
{
    // Third decision: cached or not
    if constexpr (UncachedRequest<Req>)
    {
        return resolve_request_uncached(ctx, req, std::false_type{});
    }
    else
    {
        return resolve_request_cached(ctx, req, std::false_type{});
    }
}

template<Context Ctx, Request Req>
cppcoro::task<typename Req::value_type>
resolve_request_async(Ctx& ctx, Req const& req)
{
    // Cf. the similar construct in seri_resolver_impl::resolve()
    if constexpr (!VisitableRequest<Req>)
    {
        throw std::logic_error{"request is not visitable"};
    }
    // Third decision: cached or not
    else if constexpr (UncachedRequest<Req>)
    {
        return resolve_request_uncached(ctx, req, std::true_type{});
    }
    else
    {
        return resolve_request_async_cached(ctx, req);
    }
}

template<Context Ctx, typename Val, typename Constraints>
    requires(!Request<Val>)
cppcoro::task<Val> resolve_request_local(
    Ctx& ctx, Val const& val, Constraints constraints)
{
    // async status, if appropriate, should already be FINISHED
    co_return val;
}

template<Context Ctx, Request Req, typename Constraints>
cppcoro::task<typename Req::value_type>
resolve_request_local(Ctx& ctx, Req const& req, Constraints constraints)
{
    // TODO static_assert(!req.is_proxy);
    // Second decision (based on constraints if possible): sync or async
    // This is the last time that constraints are used.
    if constexpr (constraints.force_async)
    {
        return resolve_request_async(ctx, req);
    }
    else if constexpr (constraints.force_sync)
    {
        return resolve_request_sync(ctx, req);
    }
    else
    {
        if (ctx.is_async())
        {
            return resolve_request_async(ctx, req);
        }
        else
        {
            return resolve_request_sync(ctx, req);
        }
    }
}

template<Request Req>
cppcoro::task<typename Req::value_type>
resolve_request_remote(remote_context_intf& ctx, Req const& req)
{
    co_return resolve_remote_to_value(ctx, req);
}

/*****************************************************************************
 * Public interface: resolve_request()
 */

/*
 * Resolve a non-request value; locally, whatever the context
 *
 * The "requires" here is strictly not needed: if it is omitted, and the second
 * arg is a request, then the following template would subsume.
 * However, when that template is not selected for some reason, the "requires"
 * tends to result in compiler error messages that are somewhat less obscure.
 */
template<
    Context Ctx,
    typename Val,
    typename Constraints = NoResolutionConstraints>
    requires(!Request<Val>)
cppcoro::task<Val> resolve_request(
    Ctx& ctx, Val const& val, Constraints constraints = Constraints())
{
    static_assert(!constraints.force_remote);
    // async status, if appropriate, should already be FINISHED
    co_return val;
}

/*
 * Resolves a request; remotely or locally, synchronously or asynchronously,
 * depending on context and contraints.
 *
 * Notes:
 * - The caller must ensure that the actual ctx type implements all needed
 *   context interfaces; if not, resolution will throw a "bad cast" exception.
 * - This function is blocking. Progress of an asynchronous request can be
 *   monitored via its context tree.
 * - This function throws async_cancelled when an asynchronous request is
 *   cancelled.
 * - The return type will be cppcoro::task<typename Req::value_type>
 *   if Req is a request.
 * - It seems likely that for multiple calls for the same Request, Ctx will be
 *   the same in each case (so just one template instantiation).
 */
template<
    Context Ctx,
    Request Req,
    typename Constraints = DefaultResolutionConstraints<Ctx>>
auto
resolve_request(
    Ctx& ctx, Req const& req, Constraints constraints = Constraints())
{
    static_assert(ValidContext<Ctx>);
    static_assert(!(Req::is_proxy && constraints.force_local));
    // TODO check_context_satisfies_constraints(ctx, constraints);

    // First decision (based on constraints if possible): remotely or locally.
    // A proxy request also forces remote resolving.
    if constexpr (Req::is_proxy || constraints.force_remote)
    {
        // Cast here to be like the runtime decision below
        auto& rem_ctx{cast_ctx_to_ref<remote_context_intf>(ctx)};
        return resolve_request_remote(rem_ctx, req);
    }
    else if constexpr (constraints.force_local)
    {
        // Call one of the two resolve_request_local() versions, depending on
        // Req being a plain value or a Request
        return resolve_request_local(ctx, req, constraints);
    }
    else
    {
        if (auto* rem_ctx = cast_ctx_to_ptr<remote_context_intf>(ctx))
        {
            return resolve_request_remote(*rem_ctx, req);
        }
        else
        {
            return resolve_request_local(ctx, req, constraints);
        }
    }
}

} // namespace cradle

#endif
