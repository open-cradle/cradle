#ifndef CRADLE_TESTS_INNER_SUPPORT_REQUEST_H
#define CRADLE_TESTS_INNER_SUPPORT_REQUEST_H

#include <utility>
#include <vector>

#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <cradle/inner/service/request.h>

namespace cradle {

template<typename Ctx, typename Req>
cppcoro::task<std::vector<typename Req::value_type>>
resolve_in_parallel(Ctx& ctx, std::vector<Req> const& requests)
{
    using Value = typename Req::value_type;
    std::vector<cppcoro::task<Value>> tasks;
    for (auto const& req : requests)
    {
        // Capturing ctx and req won't work for some reason
        tasks.emplace_back(
            [](Ctx& ctx, Req const& req) -> cppcoro::task<Value> {
                // Probably could use a simple return and shared_task's if
                // caching level is not none.
                co_return co_await resolve_request(ctx, req);
            }(ctx, req));
    }
    co_return co_await cppcoro::when_all(std::move(tasks));
}

} // namespace cradle

#endif
