#ifndef CRADLE_INNER_REQUESTS_FUNCTION_DEPRECATED_H
#define CRADLE_INNER_REQUESTS_FUNCTION_DEPRECATED_H

#include <functional>
#include <memory>
#include <tuple>
#include <typeinfo>
#include <utility>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/id.h>
#include <cradle/inner/core/sha256_hash_id.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/request.h>

namespace cradle {

/*
 * The "original" function requests, that have no type erasure.
 *
 * These are deprecated due to the following reasons:
 * - The request class reflects the entire request tree, and tends to grow
 *   fast. Compilation times will become much slower, and compilers will
 *   give up altogether when the tree has more than a few dozen requests.
 * - class function_request_cached stores its arguments twice: once in the
 *   request object itself, once in its captured_id member.
 * - Type-erased objects have some overhead (due to accessing the "_impl"
 *   object through a shared_ptr), but a function_request_cached object also
 *   has a shared_ptr in its captured_id member.
 * - Request identity (uuid) is not really supported.
 * - No disk caching.
 * - Memory caching is theoretically flawed (details below).
 * - No serialization.
 *
 * So normally the type-erased requests (function.h) should be preferred.
 */

template<typename Value, typename Function, typename... Args>
class function_request_uncached
{
 public:
    using element_type = function_request_uncached;
    using value_type = Value;

    static constexpr caching_level_type caching_level{
        caching_level_type::none};
    static constexpr bool introspective{false};

    function_request_uncached(Function function, Args... args)
        : function_{std::move(function)}, args_{std::move(args)...}
    {
    }

    request_uuid
    get_uuid() const
    {
        // The uuid should cover function_, but C++ does not offer anything
        // that can be used across application runs.
        throw not_implemented_error();
    }

    template<typename Ctx>
    requires ContextMatchingProps<Ctx, introspective, caching_level>
        cppcoro::task<Value>
        resolve(Ctx& ctx) const
    {
        // The "func=function_" is a workaround to prevent a gcc-10 internal
        // compiler error in release builds.
        co_return co_await std::apply(
            [&, func = function_](auto&&... args) -> cppcoro::task<Value> {
                co_return func((co_await resolve_request(
                    ctx, std::forward<decltype(args)>(args)))...);
            },
            args_);
    }

 private:
    Function function_;
    std::tuple<Args...> args_;
};

template<typename Value, typename Function, typename... Args>
struct arg_type_struct<function_request_uncached<Value, Function, Args...>>
{
    using value_type = Value;
};

template<typename Value, typename Function, typename... Args>
struct arg_type_struct<
    std::unique_ptr<function_request_uncached<Value, Function, Args...>>>
{
    using value_type = Value;
};

template<typename Value, typename Function, typename... Args>
struct arg_type_struct<
    std::shared_ptr<function_request_uncached<Value, Function, Args...>>>
{
    using value_type = Value;
};

// Due to absence of a usable uuid, these objects are suitable for memory
// caching only, and cannot be disk cached.
// (And even memory caching is not guaranteed to work, as it relies on
// type_info::name() results being unique, and this may not be so.)
template<
    caching_level_type Level,
    typename Value,
    typename Function,
    typename... Args>
requires(Level != caching_level_type::full) class function_request_cached
{
 public:
    using element_type = function_request_cached;
    using value_type = Value;

    static constexpr caching_level_type caching_level = Level;
    static constexpr bool introspective = false;

    // TODO type_info::name() "can be identical for several types"
    // (But is there an alternative? C++ can correctly compare type_info or
    // type_index values, but not map them onto a unique value.)
    function_request_cached(Function function, Args... args)
        : id_{make_captured_sha256_hashed_id(
            typeid(function).name(), args...)},
          function_{std::move(function)},
          args_{std::move(args)...}
    {
    }

    // *this and other are the same type
    bool
    equals(function_request_cached const& other) const
    {
        return *id_ == *other.id_;
    }

    bool
    less_than(function_request_cached const& other) const
    {
        return *id_ < *other.id_;
    }

    size_t
    hash() const
    {
        return id_->hash();
    }

    void
    update_hash(unique_hasher& hasher) const
    {
        id_->update_hash(hasher);
    }

    request_uuid
    get_uuid() const
    {
        // The uuid should cover function_, but C++ does not offer anything
        // that can be used across application runs.
        throw not_implemented_error();
    }

    captured_id const&
    get_captured_id() const
    {
        return id_;
    }

    template<typename Ctx>
    requires ContextMatchingProps<Ctx, introspective, caching_level>
        cppcoro::task<Value>
        resolve(Ctx& ctx) const
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
    captured_id id_;
    Function function_;
    std::tuple<Args...> args_;
};

template<
    caching_level_type Level,
    typename Value,
    typename Function,
    typename... Args>
struct arg_type_struct<
    function_request_cached<Level, Value, Function, Args...>>
{
    using value_type = Value;
};

template<
    caching_level_type Level,
    typename Value,
    typename Function,
    typename... Args>
struct arg_type_struct<
    std::unique_ptr<function_request_cached<Level, Value, Function, Args...>>>
{
    using value_type = Value;
};

template<
    caching_level_type Level,
    typename Value,
    typename Function,
    typename... Args>
struct arg_type_struct<
    std::shared_ptr<function_request_cached<Level, Value, Function, Args...>>>
{
    using value_type = Value;
};

// Used for comparing subrequests, where the main requests have the same type;
// so the subrequests have the same type too.
template<
    caching_level_type Level,
    typename Value,
    typename Function,
    typename... Args>
bool
operator==(
    function_request_cached<Level, Value, Function, Args...> const& lhs,
    function_request_cached<Level, Value, Function, Args...> const& rhs)
{
    return lhs.equals(rhs);
}

template<
    caching_level_type Level,
    typename Value,
    typename Function,
    typename... Args>
bool
operator<(
    function_request_cached<Level, Value, Function, Args...> const& lhs,
    function_request_cached<Level, Value, Function, Args...> const& rhs)
{
    return lhs.less_than(rhs);
}

template<
    caching_level_type Level,
    typename Value,
    typename Function,
    typename... Args>
size_t
hash_value(function_request_cached<Level, Value, Function, Args...> const& req)
{
    return req.hash();
}

template<
    caching_level_type Level,
    typename Value,
    typename Function,
    typename... Args>
void
update_unique_hash(
    unique_hasher& hasher,
    function_request_cached<Level, Value, Function, Args...> const& req)
{
    req.update_hash(hasher);
}

template<caching_level_type Level, typename Function, typename... Args>
requires(Level == caching_level_type::none) auto rq_function(
    Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return function_request_uncached<Value, Function, Args...>{
        std::move(function), std::move(args)...};
}

template<caching_level_type Level, typename Function, typename... Args>
requires(Level != caching_level_type::none) auto rq_function(
    Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return function_request_cached<Level, Value, Function, Args...>{
        std::move(function), std::move(args)...};
}

template<
    typename Value,
    caching_level_type Level,
    typename Function,
    typename... Args>
requires(Level != caching_level_type::none) auto rq_function(
    Function function, Args... args)
{
    return function_request_cached<Level, Value, Function, Args...>{
        std::move(function), std::move(args)...};
}

template<caching_level_type Level, typename Function, typename... Args>
requires(Level == caching_level_type::none) auto rq_function_up(
    Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_unique<
        function_request_uncached<Value, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type Level, typename Function, typename... Args>
requires(Level != caching_level_type::none) auto rq_function_up(
    Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_unique<
        function_request_cached<Level, Value, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type Level, typename Function, typename... Args>
requires(Level == caching_level_type::none) auto rq_function_sp(
    Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_shared<
        function_request_uncached<Value, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type Level, typename Function, typename... Args>
requires(Level != caching_level_type::none) auto rq_function_sp(
    Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_shared<
        function_request_cached<Level, Value, Function, Args...>>(
        std::move(function), std::move(args)...);
}

} // namespace cradle

#endif
