#ifndef CRADLE_INNER_RESOLVE_RESOLVE_REQUEST_H
#define CRADLE_INNER_RESOLVE_RESOLVE_REQUEST_H

#include <stdexcept>
#include <type_traits>
#include <utility>

#include <cppcoro/task.hpp>
#include <fmt/format.h>

#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/caching/immutable/local_locked_record.h>
#include <cradle/inner/caching/immutable/lock.h>
#include <cradle/inner/caching/immutable/ptr.h>
#include <cradle/inner/encodings/msgpack_value.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/requests/cast_ctx.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/remote.h>
#include <cradle/inner/resolve/util.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/secondary_cached_blob.h>
#include <cradle/inner/service/secondary_storage_intf.h>
#include <cradle/inner/utilities/logging.h>

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
    bool ForceAsync = false,
    bool IsSub = false>
struct ResolutionConstraints
{
    static_assert(!(ForceRemote && ForceLocal));
    static_assert(!(ForceSync && ForceAsync));
    // IsSub is relevant only for async, so set to false if ForceSync
    // (preventing unnecessary template instantiations)
    static_assert(!(ForceSync && IsSub));

    static constexpr bool force_remote = ForceRemote;
    static constexpr bool force_local = ForceLocal;
    static constexpr bool force_sync = ForceSync;
    static constexpr bool force_async = ForceAsync;
    static constexpr bool is_sub = IsSub;

    ResolutionConstraints()
    {
    }
};

using NoResolutionConstraints
    = ResolutionConstraints<false, false, false, false, false>;
using ResolutionConstraintsLocal
    = ResolutionConstraints<false, true, false, false, false>;
using ResolutionConstraintsLocalSync
    = ResolutionConstraints<false, true, true, false, false>;
using ResolutionConstraintsLocalAsyncRoot
    = ResolutionConstraints<false, true, false, true, false>;
using ResolutionConstraintsLocalAsyncSub
    = ResolutionConstraints<false, true, false, true, true>;
using ResolutionConstraintsRemoteSync
    = ResolutionConstraints<true, false, true, false, false>;
using ResolutionConstraintsRemoteAsync
    = ResolutionConstraints<true, false, false, true, false>;

// These defaults should make it superfluous for the caller to specify the
// constraints, if the actual context class is final and known at the
// resolve_request() call location.
template<Context Ctx>
using DefaultResolutionConstraints = ResolutionConstraints<
    DefinitelyRemoteContext<Ctx>,
    DefinitelyLocalContext<Ctx>,
    DefinitelySyncContext<Ctx>,
    DefinitelyAsyncContext<Ctx>,
    false>;

// A context/constraints pair where the context can be used to resolve a
// request within the limits set by the constraints.
// Due to runtime polymorphism, a mismatch can be detected at compile time only
// if the context is final.
template<typename Ctx, typename Constraints>
concept MatchingContextConstraints
    = (!(Constraints::force_remote && DefinitelyLocalContext<Ctx>)
       && !(Constraints::force_local && DefinitelyRemoteContext<Ctx>)
       && !(Constraints::force_sync && DefinitelyAsyncContext<Ctx>)
       && !(Constraints::force_async && DefinitelySyncContext<Ctx>) );

// A request/constraints pair where the request can be resolved within the
// limits set by the constraints.
template<typename Req, typename Constraints>
concept MatchingRequestConstraints
    = (!(Req::is_proxy && Constraints::force_local));

template<typename Val, typename Constraints>
    requires(!Request<Val>)
cppcoro::task<Val> resolve_request_local(
    local_context_intf& ctx,
    Val const& val,
    bool retrying,
    cache_record_lock* lock_ptr,
    Constraints constraints)
{
    // async status, if appropriate, should already be FINISHED
    co_return val;
}

template<Request Req, typename Constraints>
cppcoro::task<typename Req::value_type>
resolve_request_local(
    local_context_intf& ctx,
    Req const& req,
    bool retrying,
    cache_record_lock* lock_ptr,
    Constraints constraints)
{
    // Prepare and populate ctx if it is an async root.
    local_context_intf* new_ctx{&ctx};
    if constexpr (!constraints.is_sub && !constraints.force_sync)
    {
        bool async{};
        if constexpr (constraints.force_async)
        {
            async = true;
        }
        else
        {
            async = ctx.is_async();
        }
        if (async && !retrying)
        {
            root_local_async_context_intf* root_actx{};
            // The following cast should succeed if client uses atst_context or
            // similar
            if (auto* owner = cast_ctx_to_ptr<local_async_ctx_owner_intf>(ctx))
            {
                // (Re-)create ctx tree and root ctx; get the new root ctx
                root_actx = &owner->prepare_for_local_resolution();
                new_ctx = root_actx;
            }
            else
            {
                root_actx
                    = cast_ctx_to_ptr<root_local_async_context_intf>(ctx);
            }
            if (root_actx)
            {
                // Populate ctx with sub ctx's
                static_assert(VisitableRequest<Req>);
                req.accept(*root_actx->make_ctx_tree_builder());
            }
        }
    }

    return req.resolve(*new_ctx, lock_ptr);
}

template<Request Req>
cppcoro::task<typename Req::value_type>
resolve_request_remote_coro(
    remote_context_intf& ctx, Req const& req, cache_record_lock* lock_ptr)
{
    // This runs in co_await resolve_request().
    co_return resolve_remote_to_value(ctx, req, lock_ptr);
}

template<Request Req>
cppcoro::task<typename Req::value_type>
resolve_request_remote(
    remote_context_intf& ctx, Req const& req, cache_record_lock* lock_ptr)
{
    if (auto* owner = cast_ctx_to_ptr<remote_async_ctx_owner_intf>(ctx))
    {
        // (Re-)create ctx tree and root ctx
        owner->prepare_for_remote_resolution();
    }
    return resolve_request_remote_coro(ctx, req, lock_ptr);
}

template<
    Context Ctx,
    Request Req,
    typename Constraints = DefaultResolutionConstraints<Ctx>>
cppcoro::task<typename Req::value_type>
resolve_request_one_try(
    Ctx& ctx,
    Req const& req,
    bool retrying,
    cache_record_lock* lock_ptr,
    Constraints constraints)
{
    // First decision (based on constraints if possible): remotely or locally.
    // A proxy request also forces remote resolving.
    if constexpr (Req::is_proxy || constraints.force_remote)
    {
        // TODO if ctx is local and preparing, this throws without failing
        // ctx's preparation, leading to hangups
        auto& rem_ctx{cast_ctx_to_ref<remote_context_intf>(ctx)};
        return resolve_request_remote(rem_ctx, req, lock_ptr);
    }
    else if constexpr (constraints.force_local)
    {
        // Call one of the two resolve_request_local() versions, depending on
        // Req being a plain value or a Request
        auto& loc_ctx{cast_ctx_to_ref<local_context_intf>(ctx)};
        return resolve_request_local(
            loc_ctx, req, retrying, lock_ptr, constraints);
    }
    else
    {
        if (ctx.remotely())
        {
            auto& rem_ctx = cast_ctx_to_ref<remote_context_intf>(ctx);
            return resolve_request_remote(rem_ctx, req, lock_ptr);
        }
        else
        {
            auto& loc_ctx{cast_ctx_to_ref<local_context_intf>(ctx)};
            return resolve_request_local(
                loc_ctx, req, retrying, lock_ptr, constraints);
        }
    }
}

template<
    Context Ctx,
    RetryableRequest Req,
    typename Constraints = DefaultResolutionConstraints<Ctx>>
cppcoro::task<typename Req::value_type>
resolve_request_with_retry(
    Ctx& ctx,
    Req const& req,
    cache_record_lock* lock_ptr,
    Constraints constraints)
{
    static_assert(ValidRetryableRequest<Req>);
    int attempt = 0;
    for (;;)
    {
        std::chrono::milliseconds delay{};
        try
        {
            co_return co_await resolve_request_one_try(
                ctx, req, attempt > 0, lock_ptr, constraints);
        }
        catch (std::exception const& exc)
        {
            delay = req.handle_exception(attempt, exc);
        }
        // TODO if introspective: update status. Not really possible with
        // tasklet_tracker.
        co_await ctx.schedule_after(delay);
        ++attempt;
    }
#ifdef __cpp_lib_unreachable
    // C++23 feature
    std::unreachable();
#else
    throw std::logic_error("unreachable");
#endif
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
 * If lock_ptr is not nullptr, the call causes *lock_ptr to lock the associated
 * memory cache record. While the *lock_ptr object exists, the lock stays
 * active, and the cache record will not be evicted (so the cache keeps the
 * result in memory).
 *
 * If ctx is async, one of its prepare_for_..._resolution() functions will be
 * called:
 * - In the resolve_request() itself if not Req::retryable
 * - In the co_await resolve_request() if Req::retryable
 *
 * Notes:
 * - The caller must ensure that the actual ctx type implements all needed
 *   context interfaces; if not, resolution will throw a "bad cast" exception.
 * - This function is blocking. Progress of an asynchronous request can be
 *   monitored via its context tree.
 * - This function throws async_cancelled when an asynchronous request is
 *   cancelled.
 * - It seems likely that for multiple calls for the same Request, Ctx will be
 *   the same in each case (so just one template instantiation).
 * - Passing a non-nullptr lock_ptr is useless for uncached requests.
 * - lock_ptr will be nullptr if Req is a subrequest.
 */
template<
    Context Ctx,
    Request Req,
    typename Constraints = DefaultResolutionConstraints<Ctx>>
cppcoro::task<typename Req::value_type>
resolve_request(
    Ctx& ctx,
    Req const& req,
    Constraints constraints = Constraints(),
    cache_record_lock* lock_ptr = nullptr)
{
    static_assert(ValidContext<Ctx>);
    static_assert(MatchingContextRequest<Ctx, Req>);
    static_assert(MatchingContextConstraints<Ctx, Constraints>);
    static_assert(MatchingRequestConstraints<Req, Constraints>);

    if constexpr (Req::retryable)
    {
        return resolve_request_with_retry(ctx, req, lock_ptr, constraints);
    }
    else
    {
        return resolve_request_one_try(ctx, req, false, lock_ptr, constraints);
    }
}

// Maybe a more convenient parameter order, but not backward compatible with
// the pre-cache_record_lock situation.
template<
    Context Ctx,
    Request Req,
    typename Constraints = DefaultResolutionConstraints<Ctx>>
cppcoro::task<typename Req::value_type>
resolve_request(
    Ctx& ctx,
    Req const& req,
    cache_record_lock* lock_ptr,
    Constraints constraints = Constraints())
{
    return resolve_request(ctx, req, constraints, lock_ptr);
}

} // namespace cradle

#endif
