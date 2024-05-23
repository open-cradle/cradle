#ifndef CRADLE_INNER_RESOLVE_RESOLVE_IMPL_H
#define CRADLE_INNER_RESOLVE_RESOLVE_IMPL_H

// Resolves a function_request_impl object to a value.
// Any Req in this file is a function_request_impl instance.
// "A request" stands for a function_request_impl object.

#include <memory>
#include <utility>

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>

#include <cradle/inner/caching/immutable/local_locked_record.h>
#include <cradle/inner/caching/immutable/lock.h>
#include <cradle/inner/caching/immutable/ptr.h>
#include <cradle/inner/encodings/msgpack_value.h>
#include <cradle/inner/requests/cast_ctx.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/util.h>
#include <cradle/inner/service/secondary_cached_blob.h>
#include <cradle/inner/service/secondary_storage_intf.h>

namespace cradle {

class inner_resources;

// Resolves a request by calling the appropriate resolve_...() function.
template<typename Req>
cppcoro::task<typename Req::value_type>
resolve_request_call(local_context_intf& ctx, Req const& req)
{
    if (auto* actx = cast_ctx_to_ptr<local_async_context_intf>(ctx))
    {
        return req.resolve_async(*actx);
    }
    else
    {
        return req.resolve_sync(ctx);
    }
}

// Resolves a request by directly calling the appropriate resolve_...()
// function; with introspection if the request wants that.
template<typename Req>
    requires(!Req::introspective)
cppcoro::task<typename Req::value_type> resolve_request_direct(
    local_context_intf& ctx, Req const& req)
{
    return resolve_request_call(ctx, req);
}

template<typename Req>
    requires(Req::introspective)
cppcoro::task<typename Req::value_type> resolve_request_direct(
    local_context_intf& ctx, Req const& req)
{
    auto& intr_ctx = cast_ctx_to_ref<introspective_context_intf>(ctx);
    co_await dummy_coroutine();
    coawait_introspection guard{
        intr_ctx,
        "resolve_request",
        fmt::format("{}/call", req.get_introspection_title())};
    co_return co_await resolve_request_call(ctx, req);
}

// Resolves a memory-cached request using some sort of secondary cache.
// A memory-cached request needs no secondary cache, so it can be resolved
// right away (by calling the request's function).
template<typename Req>
    requires(is_memory_cached(Req::caching_level))
cppcoro::task<typename Req::value_type> resolve_secondary_cached(
    caching_context_intf& ctx, Req const& req)
{
    return resolve_request_direct(ctx, req);
}

// Resolves a fully-cached request using some sort of secondary cache, and some
// sort of serialization.
template<typename Req>
    requires(is_fully_cached(Req::caching_level))
cppcoro::task<typename Req::value_type> resolve_secondary_cached(
    caching_context_intf& ctx, Req const& req)
{
    using Value = typename Req::value_type;
    inner_resources& resources{ctx.get_resources()};
    auto& cache = resources.secondary_cache();
    bool allow_blob_files = cache.allow_blob_files();
    auto create_blob_task = [&]() -> cppcoro::task<blob> {
        co_return serialize_value(
            co_await resolve_request_direct(ctx, req), allow_blob_files);
    };
    co_return deserialize_value<Value>(co_await secondary_cached_blob(
        resources, req.get_captured_id(), std::move(create_blob_task)));
}

// Called if the action cache contains no record for this request.
// Resolves the request, stores the result in the CAS, updates the action
// cache. The cache is accessed via ptr. The caller should ensure that ctx, req
// and ptr outlive the coroutine.
template<typename Req>
    requires(is_cached(Req::caching_level))
cppcoro::shared_task<void> resolve_request_on_memory_cache_miss(
    caching_context_intf& ctx,
    Req const& req,
    immutable_cache_ptr<typename Req::value_type>& ptr)
{
    try
    {
        ptr.record_value(co_await resolve_secondary_cached(ctx, req));
    }
    catch (...)
    {
        ptr.record_failure();
        throw;
    }
}

// Resolves a request, with caching, and with or without introspection,
// depending on the request's compile-time attributes.
template<typename Req>
    requires(is_cached(Req::caching_level) && !Req::value_based_caching)
cppcoro::task<typename Req::value_type> resolve_request_cached(
    caching_context_intf& ctx, Req const& req, cache_record_lock* lock_ptr)
{
    using value_type = typename Req::value_type;
    using ptr_type = immutable_cache_ptr<value_type>;
    // While ptr lives, the corresponding cache record lives too.
    // ptr lives until the shared_task has run (on behalf of the current
    // request, or a previous one), and the value has been retrieved from the
    // cache record.
    ptr_type ptr{
        ctx.get_resources().memory_cache(),
        req.get_captured_id(),
        [&ctx, &req](untyped_immutable_cache_ptr& ptr) {
            return resolve_request_on_memory_cache_miss(
                ctx, req, static_cast<ptr_type&>(ptr));
        }};
    if (lock_ptr != nullptr)
    {
        lock_ptr->set_record(
            std::make_unique<local_locked_cache_record>(ptr.get_record()));
    }
    if constexpr (Req::introspective)
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
    // If function ran, status already will be FINISHED
    // If result came from cache, it will not yet be
    if (auto* actx = cast_ctx_to_ptr<local_async_context_intf>(ctx))
    {
        actx->update_status(async_status::FINISHED);
    }
    // Finally, return the shared_task's value.
    co_return ptr.get_value();
}

template<typename Req>
    requires(is_cached(Req::caching_level) && Req::value_based_caching)
cppcoro::task<typename Req::value_type> resolve_request_cached(
    caching_context_intf& ctx, Req const& req, cache_record_lock* lock_ptr)
{
    // Make a composition-based variant of req that has all
    // subrequests resolved and replaced by resulting values; then resolve
    // that request as any other request, using composition-based caching.
    auto clone{co_await req.make_flattened_clone(ctx)};
    co_return co_await resolve_request_cached(ctx, *clone, lock_ptr);
}

// Resolves a request, with or without caching, with or without introspection,
// depending on the request's compile-time attributes.
// Called from function_request_impl::resolve().
template<typename Req>
cppcoro::task<typename Req::value_type>
resolve_impl(
    local_context_intf& ctx, Req const& req, cache_record_lock* lock_ptr)
{
    // Second decision: cached or not
    if constexpr (is_uncached(Req::caching_level))
    {
        return resolve_request_direct(ctx, req);
    }
    else
    {
        auto& cac_ctx = cast_ctx_to_ref<caching_context_intf>(ctx);
        return resolve_request_cached(cac_ctx, req, lock_ptr);
    }
}

} // namespace cradle

#endif
