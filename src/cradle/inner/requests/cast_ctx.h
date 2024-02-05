#ifndef CRADLE_INNER_REQUESTS_CAST_CTX_H
#define CRADLE_INNER_REQUESTS_CAST_CTX_H

#include <concepts>
#include <memory>
#include <stdexcept>

#include <cradle/inner/requests/generic.h>

namespace cradle {

// Casts a context_intf* to another element in the context_intf class tree.
// By default uses a slow dynamic_cast.
// Uses a much faster to_..._context_intf() member function if available for
// the destination class.
template<typename DestCtx>
struct dynamic_ctx_caster
{
    static DestCtx*
    cast_ptr(context_intf* ctx)
    {
        return dynamic_cast<DestCtx*>(ctx);
    }
};

template<>
struct dynamic_ctx_caster<local_context_intf>
{
    static local_context_intf*
    cast_ptr(context_intf* ctx)
    {
        return ctx->to_local_context_intf();
    }
};

template<>
struct dynamic_ctx_caster<remote_context_intf>
{
    static remote_context_intf*
    cast_ptr(context_intf* ctx)
    {
        return ctx->to_remote_context_intf();
    }
};

template<>
struct dynamic_ctx_caster<sync_context_intf>
{
    static sync_context_intf*
    cast_ptr(context_intf* ctx)
    {
        return ctx->to_sync_context_intf();
    }
};

template<>
struct dynamic_ctx_caster<async_context_intf>
{
    static async_context_intf*
    cast_ptr(context_intf* ctx)
    {
        return ctx->to_async_context_intf();
    }
};

template<>
struct dynamic_ctx_caster<local_async_context_intf>
{
    static local_async_context_intf*
    cast_ptr(context_intf* ctx)
    {
        return ctx->to_local_async_context_intf();
    }
};

template<>
struct dynamic_ctx_caster<remote_async_context_intf>
{
    static remote_async_context_intf*
    cast_ptr(context_intf* ctx)
    {
        return ctx->to_remote_async_context_intf();
    }
};

template<>
struct dynamic_ctx_caster<caching_context_intf>
{
    static caching_context_intf*
    cast_ptr(context_intf* ctx)
    {
        return ctx->to_caching_context_intf();
    }
};

template<>
struct dynamic_ctx_caster<introspective_context_intf>
{
    static introspective_context_intf*
    cast_ptr(context_intf* ctx)
    {
        return ctx->to_introspective_context_intf();
    }
};

// Casts a context_intf reference to DestCtx*.
// Returns nullptr if the runtime type doesn't match.
// Retains the original type if no cast is needed.
template<Context DestCtx, Context SrcCtx>
    requires(std::convertible_to<SrcCtx&, DestCtx&>)
SrcCtx* cast_ctx_to_ptr_base(SrcCtx& ctx) noexcept
{
    return &ctx;
}

template<Context DestCtx, Context SrcCtx>
    requires(!std::convertible_to<SrcCtx&, DestCtx&>)
DestCtx* cast_ctx_to_ptr_base(SrcCtx& ctx) noexcept
{
    return dynamic_ctx_caster<DestCtx>::cast_ptr(&ctx);
}

// Casts a context_intf reference to DestCtx*.
// Returns nullptr if the runtime type doesn't match.
// Returns nullptr if the remotely() and/or is_async() return values don't
// match; see comments on throw_on_ctx_mismatch() for details;
// these function are not called when compile-time information suffices.
// Retains the original type if no cast is needed.
template<Context DestCtx, Context SrcCtx>
DestCtx*
cast_ctx_to_ptr(SrcCtx& ctx) noexcept
{
    if constexpr (
        RemoteContext<DestCtx> && !LocalContext<DestCtx>
        && !DefinitelyRemoteContext<SrcCtx>)
    {
        if constexpr (DefinitelyLocalContext<SrcCtx>)
        {
            return nullptr;
        }
        else if (!ctx.remotely())
        {
            return nullptr;
        }
    }
    if constexpr (
        LocalContext<DestCtx> && !RemoteContext<DestCtx>
        && !DefinitelyLocalContext<SrcCtx>)
    {
        if constexpr (DefinitelyRemoteContext<SrcCtx>)
        {
            return nullptr;
        }
        else if (ctx.remotely())
        {
            return nullptr;
        }
    }
    if constexpr (
        AsyncContext<DestCtx> && !SyncContext<DestCtx>
        && !DefinitelyAsyncContext<SrcCtx>)
    {
        if constexpr (DefinitelySyncContext<SrcCtx>)
        {
            return nullptr;
        }
        else if (!ctx.is_async())
        {
            return nullptr;
        }
    }
    if constexpr (
        SyncContext<DestCtx> && !AsyncContext<DestCtx>
        && !DefinitelySyncContext<SrcCtx>)
    {
        if constexpr (DefinitelyAsyncContext<SrcCtx>)
        {
            return nullptr;
        }
        else if (ctx.is_async())
        {
            return nullptr;
        }
    }
    return cast_ctx_to_ptr_base<DestCtx>(ctx);
}

// Casts a context_intf reference to DestCtx&.
// Retains the original type if no cast is needed.
template<Context DestCtx, Context SrcCtx>
    requires(std::convertible_to<SrcCtx&, DestCtx&>)
SrcCtx& cast_ctx_to_ref_base(SrcCtx& ctx)
{
    return ctx;
}

template<Context DestCtx, Context SrcCtx>
    requires(!std::convertible_to<SrcCtx&, DestCtx&>)
DestCtx& cast_ctx_to_ref_base(SrcCtx& ctx)
{
    return *dynamic_ctx_caster<DestCtx>::cast_ptr(&ctx);
}

/*
 * Throws when ctx.remotely() or ctx.is_async() return values conflict with
 * a specified context cast.
 *
 * E.g., when we have a cast like
 *   auto& lctx = cast_ctx_to_ref<local_context_intf>(ctx);
 * we want resolution to happen locally only, so ctx.remotely() should be
 * returning false.
 *
 * The remotely() return value need not be checked in three situations:
 * - When casting to a context type covering both local and remote
 *   execution, then either return value is OK, and the cast will succeed.
 * - When the source context is definitely remote-only, we already know that
 *   ctx.remotely() should be returning true, so the the cast will succeed.
 * - When the source context is definitely local-only, we already know that
 *   ctx.remotely() should be returning false, so the cast is not possible.
 *
 * The type of the thrown exceptions is std::logic_error. std::bad_cast looks
 * more appropriate but has no constructor taking a string.
 */
template<Context DestCtx, Context SrcCtx>
void
throw_on_ctx_mismatch(SrcCtx& ctx)
{
    if constexpr (
        RemoteContext<DestCtx> && !LocalContext<DestCtx>
        && !DefinitelyRemoteContext<SrcCtx>)
    {
        // The first throw depends on constexpr values only.
        //   static_assert(!DefinitelyLocalContext<SrcCtx>);
        // would also be possible but cannot be unit tested
        // (as the test code won't compile).
        if constexpr (DefinitelyLocalContext<SrcCtx>)
        {
            throw std::logic_error("DefinitelyLocalContext");
        }
        else if (!ctx.remotely())
        {
            throw std::logic_error("remotely() returning false");
        }
    }
    if constexpr (
        LocalContext<DestCtx> && !RemoteContext<DestCtx>
        && !DefinitelyLocalContext<SrcCtx>)
    {
        if constexpr (DefinitelyRemoteContext<SrcCtx>)
        {
            throw std::logic_error("DefinitelyRemoteContext");
        }
        else if (ctx.remotely())
        {
            throw std::logic_error("remotely() returning true");
        }
    }
    if constexpr (
        AsyncContext<DestCtx> && !SyncContext<DestCtx>
        && !DefinitelyAsyncContext<SrcCtx>)
    {
        if constexpr (DefinitelySyncContext<SrcCtx>)
        {
            throw std::logic_error("DefinitelySyncContext");
        }
        else if (!ctx.is_async())
        {
            throw std::logic_error("is_async() returning false");
        }
    }
    if constexpr (
        SyncContext<DestCtx> && !AsyncContext<DestCtx>
        && !DefinitelySyncContext<SrcCtx>)
    {
        if constexpr (DefinitelyAsyncContext<SrcCtx>)
        {
            throw std::logic_error("DefinitelyAsyncContext");
        }
        else if (ctx.is_async())
        {
            throw std::logic_error("is_async() returning true");
        }
    }
}

// Casts a context_intf reference to DestCtx&.
// Throws if the runtime type doesn't match.
// Throws if the remotely() and/or is_async() return values don't match.
// These function are not called when compile-time information suffices,
// leading to a throw depending on constexpr values only, and a C4702
// warning in a VS2019 release build.
// Retains the original type if no cast is needed.
template<Context DestCtx, Context SrcCtx>
DestCtx&
cast_ctx_to_ref(SrcCtx& ctx)
{
    throw_on_ctx_mismatch<DestCtx>(ctx);
    return cast_ctx_to_ref_base<DestCtx>(ctx);
}

// Casts a shared_ptr<context_intf> to shared_ptr<DestCtx>.
// Throws if the source shared_ptr is empty.
// Throws if the runtime type doesn't match.
// Throws if the remotely() and/or is_async() return values don't match;
// these function are not called when compile-time information suffices.
template<Context DestCtx, Context SrcCtx>
std::shared_ptr<DestCtx>
cast_ctx_to_shared_ptr(std::shared_ptr<SrcCtx> const& ctx)
{
    throw_on_ctx_mismatch<DestCtx>(*ctx);
    return std::dynamic_pointer_cast<DestCtx>(ctx);
}

} // namespace cradle

#endif
