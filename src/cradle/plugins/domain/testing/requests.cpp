#include <cstdlib>

#include <spdlog/spdlog.h>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/requests/cast_ctx.h>
#include <cradle/plugins/domain/testing/requests.h>

namespace cradle {

// TODO ensure that shared memory works properly even if not on an RPC server
cppcoro::task<blob>
make_some_blob(context_intf& ctx, std::size_t size, bool use_shared_memory)
{
    spdlog::get("cradle")->info(
        "make_some_blob({}, {})", size, use_shared_memory);
    auto& loc_ctx{cast_ctx_to_ref<local_context_intf>(ctx)};
    auto owner = loc_ctx.make_data_owner(size, use_shared_memory);
    auto* data = owner->data();
    uint8_t v = 0;
    for (std::size_t i = 0; i < size; ++i)
    {
        data[i] = v;
        v = v * 3 + 1;
    }
    // TODO where is the loc_ctx.on_value_complete() call?
    co_return blob{std::move(owner), as_bytes(data), size};
}

cppcoro::task<int>
cancellable_coro(context_intf& ctx, int loops, int delay)
{
    auto& cctx{cast_ctx_to_ref<local_async_context_intf>(ctx)};
    async_id ctx_id{cctx.get_id()};
    auto logger = spdlog::get("cradle");
    logger->info(
        "cancellable_coro(ctx {}, loops={}, delay={})", ctx_id, loops, delay);
    int i = 0;
    for (; i < std::abs(loops); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        if (cctx.is_cancellation_requested())
        {
            logger->info(
                "cancellable_coro(ctx {}): throwing cancelled", ctx_id);
            cctx.throw_async_cancelled();
        }
    }
    if (loops < 0)
    {
        logger->info("cancellable_coro(ctx {}): throwing error", ctx_id);
        throw async_error{"cancellable_coro() failed"};
    }
    int res = i + delay;
    logger->info("cancellable_coro(ctx {}): co_return {}", ctx_id, res);
    co_return res;
}

int
non_cancellable_func(int loops, int delay)
{
    auto logger = spdlog::get("cradle");
    logger->info("non_cancellable_func(loops={}, delay={})", loops, delay);
    int i = 0;
    for (; i < std::abs(loops); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
    if (loops < 0)
    {
        logger->info("non_cancellable_func(): throwing error");
        throw async_error{"non_cancellable_func() failed"};
    }
    int res = i + delay;
    logger->info("non_cancellable_func(): return {}", res);
    return res;
}

} // namespace cradle
