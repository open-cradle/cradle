#include <tuple>
#include <type_traits>

#include <cppcoro/task.hpp>
#include <cppcoro/when_all_ready.hpp>

#include <cradle/inner/core/id.h>

namespace cradle {

enum class caching_level
{
    none,
    memory,
    disk
};

template<class Value, caching_level CachingLevel>
struct request
{
    using value_type = Value;

    static constexpr cradle::caching_level caching_level = CachingLevel;
};

struct request_resolution_context
{
    // TODO: Define the contents of this...
};

template<class Request>
cppcoro::task<typename Request::value_type>
resolve_request(request_resolution_context& ctx, Request const& request)
{
    // TODO: Caching, etc.
    co_return co_await request.resolve(ctx);
}

template<class Value>
struct value_request : request<Value, caching_level::none>
{
    value_request(Value value) : value_(value)
    {
    }

    // TODO: Decide what to do about IDs.
    captured_id const&
    id() const
    {
        return id_;
    }

    cppcoro::task<Value>
    resolve(request_resolution_context& ctx) const
    {
        co_return value_;
    }

    // TODO: Introspection API.

 private:
    Value value_;
    // TODO: Decide what to do about IDs.
    captured_id id_;
};

namespace rq {

template<class Value>
value_request<Value>
value(Value value)
{
    return value_request<Value>(std::move(value));
}

} // namespace rq

template<caching_level CachingLevel, class Function, class... Args>
struct function_request
    : request<
          std::invoke_result_t<Function, typename Args::value_type...>,
          CachingLevel>
{
    function_request(Function function, Args... args)
        : function_(std::move(function)), args_(std::move(args)...)
    {
    }

    // TODO: Decide what to do about IDs.
    captured_id const&
    id() const
    {
        return id_;
    }

    cppcoro::task<typename function_request::value_type>
    resolve(request_resolution_context& ctx) const
    {
        co_return co_await std::apply(
            [&](auto&&... args)
                -> cppcoro::task<typename function_request::value_type> {
                // TODO: Wait on args in parallel.
                co_return function_((co_await resolve_request(ctx, args))...);
            },
            args_);
    }

    // TODO: Introspection API.

 private:
    Function function_;
    std::tuple<Args...> args_;
    // TODO: Decide what to do about IDs.
    captured_id id_;
};

namespace rq {

template<caching_level CachingLevel, class Function, class... Args>
function_request<CachingLevel, Function, Args...>
function(Function function, Args... args)
{
    return function_request<CachingLevel, Function, Args...>(
        std::move(function), std::move(args)...);
}

} // namespace rq

} // namespace cradle

#include <cppcoro/sync_wait.hpp>

#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("request example", "[external]")
{
    request_resolution_context ctx;
    REQUIRE(cppcoro::sync_wait(resolve_request(ctx, rq::value(6))) == 6);

    auto add = [](int a, int b) { return a + b; };
    REQUIRE(
        cppcoro::sync_wait(resolve_request(
            ctx,
            rq::function<caching_level::memory>(
                add, rq::value(6), rq::value(1))))
        == 7);
}
