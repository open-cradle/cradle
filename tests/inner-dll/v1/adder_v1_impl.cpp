#include <chrono>
#include <stdexcept>
#include <thread>

#include <spdlog/spdlog.h>

#include "adder_v1_defs.h"
#include "adder_v1_impl.h"
#include <cradle/inner/requests/cast_ctx.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

int
adder_v1_func(int a, int b)
{
    if (b == adder_v1_b_throw)
    {
        throw std::invalid_argument{"adder_v1_b_throw"};
    }
    if (b == adder_v1_b_crash)
    {
        return *(volatile int*) 0;
    }
    return a + b;
}

cppcoro::task<int>
coro_v1_func(context_intf& ctx, int loops, int delay)
{
    auto& cctx{cast_ctx_to_ref<local_async_context_intf>(ctx)};
    async_id ctx_id{cctx.get_id()};
    // spdlog::get() may return nullptr (seen with Clang release on CI)
    auto logger = ensure_logger("cradle");
    logger->info(
        "coro_v1_func(ctx {}, loops={}, delay={})", ctx_id, loops, delay);
    int i = 0;
    for (; i < std::abs(loops); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        if (cctx.is_cancellation_requested())
        {
            logger->info("coro_v1_func(ctx {}): throwing cancelled", ctx_id);
            cctx.throw_async_cancelled();
        }
    }
    if (loops < 0)
    {
        logger->info("coro_v1_func(ctx {}): throwing error", ctx_id);
        throw async_error{"coro_v1_func() failed"};
    }
    int res = i + delay;
    logger->info("coro_v1_func(ctx {}): co_return {}", ctx_id, res);
    co_return res;
}

} // namespace cradle
