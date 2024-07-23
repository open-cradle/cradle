#include <chrono>
#include <exception>
#include <thread>

#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

#include "cancel_async.h"
#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

namespace {

// Requests cancellation of all coroutines sharing ctx
cppcoro::task<void>
cancel_async_checker_coro(async_context_intf& ctx, spdlog::logger& logger)
{
    logger.info("cancel_async_checker_coro(ctx {})", ctx.get_id());
    for (int i = 0; i < 20; ++i)
    {
        auto status = co_await ctx.get_status_coro();
        logger.info("cancel_async_checker_coro {}: {}", i, status);
        if (status == async_status::FINISHED)
        {
            logger.error("finished too early");
            break;
        }
        if (status == async_status::CANCELLED)
        {
            break;
        }
        if (i == 8)
        {
            logger.info("cancel_async_checker_coro {}: cancelling", i);
            co_await ctx.request_cancellation_coro();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    co_return;
}

} // namespace

void
cancel_async_checker_func(async_context_intf& ctx)
{
    // Running on checker_thread, so would call terminate() in case of an
    // unhandled exception.
    auto logger = ensure_logger("checker");
    try
    {
        cppcoro::sync_wait(cancel_async_checker_coro(ctx, *logger));
    }
    catch (std::exception const& exc)
    {
        logger->error("cancel_async_checker_func() caught {}", exc.what());
    }
}

} // namespace cradle
