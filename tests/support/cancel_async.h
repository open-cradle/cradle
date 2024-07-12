#ifndef CRADLE_TESTS_SUPPORT_CANCEL_ASYNC_H
#define CRADLE_TESTS_SUPPORT_CANCEL_ASYNC_H

#include <functional>
#include <string>
#include <thread>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/plugins/domain/testing/context.h>

namespace cradle {

class inner_resources;

void
cancel_async_checker_func(async_context_intf& ctx);

void
test_cancel_async(
    inner_resources& resources,
    std::string const& proxy_name,
    Request auto const& req)
{
    // Run the checker function on a separate thread, independent from the
    // ones under test.
    // One reason for preferring std::jthread over std::thread is that
    // std::thread::~thread() calls terminate() if the thread wasn't
    // joined; e.g. if the test code threw.
    atst_context ctx{resources, proxy_name};
    std::jthread checker_thread(cancel_async_checker_func, std::ref(ctx));
    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(resolve_request(ctx, req)), async_cancelled);
    REQUIRE(
        cppcoro::sync_wait(ctx.get_status_coro()) == async_status::CANCELLED);
    checker_thread.join();
}

} // namespace cradle

#endif
