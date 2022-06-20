#ifndef CRADLE_INNER_REQUESTS_FUNCTION_H
#define CRADLE_INNER_REQUESTS_FUNCTION_H

#include <memory>
#include <utility>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/core.h>

namespace cradle {

template<class Function, class... Args>
class function_request_uncached
{
 public:
    using element_type = function_request_uncached;
    using value_type = std::
        invoke_result_t<Function, typename Args::element_type::value_type...>;

    static constexpr caching_level_type caching_level
        = caching_level_type::none;

    function_request_uncached(Function function, Args... args)
        : function_{std::move(function)}, args_{std::move(args)...}
    {
    }

    template<UncachedContext Ctx>
    cppcoro::task<value_type>
    resolve(Ctx& ctx) const
    {
        // The "func=function_" is a workaround to prevent a gcc-10 internal
        // compiler error in release builds.
        co_return co_await std::apply(
            [&, func = function_](auto&&... args)
                -> cppcoro::task<
                    typename function_request_uncached::value_type> {
                co_return func((co_await resolve_request(ctx, args))...);
            },
            args_);
    }

 private:
    Function function_;
    std::tuple<Args...> args_;
};

template<caching_level_type level, class Function, class... Args>
class function_request_cached
{
 public:
    using element_type = function_request_cached;
    using value_type = std::
        invoke_result_t<Function, typename Args::element_type::value_type...>;

    static constexpr caching_level_type caching_level = level;

    function_request_cached(Function function, Args... args)
        : function_{std::move(function)}, args_{std::move(args)...}
    {
        create_id();
    }

    captured_id const&
    get_captured_id() const
    {
        return id_;
    }

    template<CachedContext Ctx>
    cppcoro::task<value_type>
    resolve(Ctx& ctx) const
    {
        // The "func=function_" is a workaround to prevent a gcc-10 internal
        // compiler error in release builds.
        co_return co_await std::apply(
            [&, func = function_](auto&&... args)
                -> cppcoro::task<
                    typename function_request_cached::value_type> {
                co_return func((co_await resolve_request(ctx, args))...);
            },
            args_);
    }

 private:
    Function function_;
    std::tuple<Args...> args_;
    captured_id id_;

    void
    create_id()
    {
        // TODO
        id_ = make_captured_id(&function_);
    }
};

template<typename Value, class Function, class... Args>
class function_request_uncached_impl : public uncached_request_intf<Value>
{
 public:
    function_request_uncached_impl(Function function, Args... args)
        : function_{std::move(function)}, args_{std::move(args)...}
    {
    }

    cppcoro::task<Value>
    resolve(uncached_context_intf& ctx) const override
    {
        // The "func=function_" is a workaround to prevent a gcc-10 internal
        // compiler error in release builds.
        co_return co_await std::apply(
            [&, func = function_](auto&&... args) -> cppcoro::task<Value> {
                co_return func((co_await resolve_request(ctx, args))...);
            },
            args_);
    }

 private:
    Function function_;
    std::tuple<Args...> args_;
};

template<typename Value>
class function_request_uncached_erased
{
 public:
    using element_type = function_request_uncached_erased;
    using value_type = Value;

    static constexpr caching_level_type caching_level
        = caching_level_type::none;

    template<typename Function, typename... Args>
    function_request_uncached_erased(Function function, Args... args)
        : impl_{std::make_shared<
            function_request_uncached_impl<Value, Function, Args...>>(
            std::move(function), std::move(args)...)}
    {
    }

    cppcoro::task<value_type>
    resolve(uncached_context_intf& ctx) const
    {
        return impl_->resolve(ctx);
    }

 private:
    std::shared_ptr<uncached_request_intf<Value>> impl_;
};

template<typename Value, class Function, class... Args>
class function_request_cached_impl : public cached_request_intf<Value>
{
 public:
    function_request_cached_impl(Function function, Args... args)
        : function_{std::move(function)}, args_{std::move(args)...}
    {
        create_id();
    }

    captured_id const&
    get_captured_id() const override
    {
        return id_;
    }

    cppcoro::task<Value>
    resolve(cached_context_intf& ctx) const override
    {
        // The "func=function_" is a workaround to prevent a gcc-10 internal
        // compiler error in release builds.
        co_return co_await std::apply(
            [&, func = function_](auto&&... args) -> cppcoro::task<Value> {
                co_return func((co_await resolve_request(ctx, args))...);
            },
            args_);
    }

 private:
    Function function_;
    std::tuple<Args...> args_;
    captured_id id_;

    void
    create_id()
    {
        // TODO
        id_ = make_captured_id(&function_);
    }
};

template<caching_level_type level, typename Value>
class function_request_cached_erased
{
 public:
    using element_type = function_request_cached_erased;
    using value_type = Value;

    static constexpr caching_level_type caching_level = level;

    template<typename Function, typename... Args>
    function_request_cached_erased(Function function, Args... args)
        : impl_{std::make_shared<
            function_request_cached_impl<Value, Function, Args...>>(
            std::move(function), std::move(args)...)}
    {
    }

    captured_id const&
    get_captured_id() const
    {
        return impl_->get_captured_id();
    }

    cppcoro::task<value_type>
    resolve(cached_context_intf& ctx) const
    {
        return impl_->resolve(ctx);
    }

 private:
    std::shared_ptr<cached_request_intf<Value>> impl_;
};

template<caching_level_type level, class Function, class... Args>
requires(level == caching_level_type::none) auto rq_function(
    Function function, Args... args)
{
    return function_request_uncached<Function, Args...>{
        std::move(function), std::move(args)...};
}

template<caching_level_type level, class Function, class... Args>
requires(level != caching_level_type::none) auto rq_function(
    Function function, Args... args)
{
    return function_request_cached<level, Function, Args...>{
        std::move(function), std::move(args)...};
}

template<caching_level_type level, class Function, class... Args>
requires(level == caching_level_type::none) auto rq_function_up(
    Function function, Args... args)
{
    return std::make_unique<function_request_uncached<Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type level, class Function, class... Args>
requires(level != caching_level_type::none) auto rq_function_up(
    Function function, Args... args)
{
    return std::make_unique<function_request_cached<level, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type level, class Function, class... Args>
requires(level == caching_level_type::none) auto rq_function_sp(
    Function function, Args... args)
{
    return std::make_shared<function_request_uncached<Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type level, class Function, class... Args>
requires(level != caching_level_type::none) auto rq_function_sp(
    Function function, Args... args)
{
    return std::make_shared<function_request_cached<level, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type level, class Function, class... Args>
requires(level == caching_level_type::none) auto rq_function_erased(
    Function function, Args... args)
{
    using value_type = std::
        invoke_result_t<Function, typename Args::element_type::value_type...>;
    return function_request_uncached_erased<value_type>{
        std::move(function), std::move(args)...};
}

template<caching_level_type level, class Function, class... Args>
requires(level != caching_level_type::none) auto rq_function_erased(
    Function function, Args... args)
{
    using value_type = std::
        invoke_result_t<Function, typename Args::element_type::value_type...>;
    return function_request_cached_erased<level, value_type>{
        std::move(function), std::move(args)...};
}

} // namespace cradle

#endif
