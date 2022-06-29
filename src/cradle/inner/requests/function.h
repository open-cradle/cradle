#ifndef CRADLE_INNER_REQUESTS_FUNCTION_H
#define CRADLE_INNER_REQUESTS_FUNCTION_H

#include <memory>
#include <optional>
#include <typeinfo>
#include <utility>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/id.h>
#include <cradle/inner/core/sha256_hash_id.h>
#include <cradle/inner/core/unique_hash.h>
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
    static constexpr bool introspective = false;

    function_request_cached(Function function, Args... args)
        : id_{make_captured_sha256_hashed_id(
            typeid(function).name(), args...)},
          function_{std::move(function)},
          args_{std::move(args)...}
    {
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
    captured_id id_;
    Function function_;
    std::tuple<Args...> args_;
};

template<typename Value>
class function_request_intf : public id_interface
{
 public:
    virtual ~function_request_intf() = default;

    virtual cppcoro::task<Value>
    resolve(context_intf& ctx) const = 0;
};

// - This class implements id_interface exactly like sha256_hashed_id
// - The major advantage is that args_ need not be copied
// TODO how can we deserialize these objects? Creator needs to
// provide all template arguments.
template<typename Value, class Function, class... Args>
class function_request_impl : public function_request_intf<Value>
{
 public:
    function_request_impl(Function function, Args... args)
        : function_{std::move(function)},
          // TODO function_id_ not guaranteed to be unique.
          // Use &function_ instead?
          function_id_{typeid(function).name()},
          args_{std::move(args)...}
    {
    }

    bool
    equals(function_request_impl const& other) const
    {
        return function_id_ == other.function_id_ && args_ == other.args_;
    }

    bool
    less_than(function_request_impl const& other) const
    {
        if (function_id_ != other.function_id_)
        {
            return function_id_ < other.function_id_;
        }
        return args_ < other.args_;
    }

    bool
    equals(id_interface const& other) const override
    {
        auto const& other_id
            = static_cast<function_request_impl const&>(other);
        return equals(other_id);
    }

    bool
    less_than(id_interface const& other) const override
    {
        auto const& other_id
            = static_cast<function_request_impl const&>(other);
        return less_than(other_id);
    }

    // Maybe caching the hashes could be optional (policy?).
    size_t
    hash() const override
    {
        if (!hash_)
        {
            auto function_hash = invoke_hash(function_id_);
            auto args_hash = std::apply(
                [](auto&&... args) {
                    return combine_hashes(invoke_hash(args)...);
                },
                args_);
            hash_ = combine_hashes(function_hash, args_hash);
        }
        return *hash_;
    }

    std::string
    get_unique_hash() const override
    {
        if (unique_hash_.empty())
        {
            unique_hash_ = id_interface::get_unique_hash();
        }
        return unique_hash_;
    }

    void
    update_hash(unique_hasher& hasher) const override
    {
        hasher.using_lambda();
        hasher.encode_type<Function>();
        std::apply(
            [&hasher](auto&&... args) {
                (update_unique_hash(hasher, args), ...);
            },
            args_);
    }

    cppcoro::task<Value>
    resolve(context_intf& ctx) const override
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
    std::string function_id_;
    std::tuple<Args...> args_;
    mutable std::optional<size_t> hash_;
    mutable std::string unique_hash_;
};

template<caching_level_type level, typename Value, bool introspective_>
class function_request_erased
{
 public:
    using element_type = function_request_erased;
    using value_type = Value;

    static constexpr caching_level_type caching_level = level;
    static constexpr bool introspective = introspective_;

    template<typename Function, typename... Args>
    requires(!introspective)
        function_request_erased(Function function, Args... args)
        : obj_id_(next_id_++)
    {
        auto impl{
            std::make_shared<function_request_impl<Value, Function, Args...>>(
                std::move(function), std::move(args)...)};
        impl_ = impl;
        if constexpr (caching_level != caching_level_type::none)
        {
            captured_id_ = captured_id{impl};
        }
    }

    template<typename Function, typename... Args>
    requires(introspective) function_request_erased(
        std::string const& title, Function function, Args... args)
        : obj_id_(next_id_++), title_{title}
    {
        auto impl{
            std::make_shared<function_request_impl<Value, Function, Args...>>(
                std::move(function), std::move(args)...)};
        impl_ = impl;
        if constexpr (caching_level != caching_level_type::none)
        {
            captured_id_ = captured_id{impl};
        }
    }

    int
    obj_id() const
    {
        return obj_id_;
    }

    bool
    equals(function_request_erased const& other) const
    {
        return impl_->equals(*other.impl_);
    }

    bool
    less_than(function_request_erased const& other) const
    {
        return impl_->less_than(*other.impl_);
    }

    size_t
    hash() const
    {
        // TODO combine with caching_level?
        return impl_->hash();
    }

    void
    update_hash(unique_hasher& hasher) const
    {
        impl_->update_hash(hasher);
    }

    captured_id const&
    get_captured_id() const
    {
        if constexpr (caching_level == caching_level_type::none)
        {
            throw not_implemented_error(
                "captured_id only available for cached function requests");
        }
        return captured_id_;
    }

    cppcoro::task<value_type>
    resolve(context_intf& ctx) const
    {
        return impl_->resolve(ctx);
    }

    std::string
    get_introspection_title() const requires(introspective)
    {
        return title_;
    }

 private:
    inline static int next_id_{};
    int obj_id_;
    std::string title_;
    std::shared_ptr<function_request_intf<Value>> impl_;
    captured_id captured_id_;
};

template<
    caching_level_type lhs_level,
    caching_level_type rhs_level,
    typename Value,
    bool lhs_intrsp,
    bool rhs_intrsp>
bool
operator==(
    function_request_erased<lhs_level, Value, lhs_intrsp> const& lhs,
    function_request_erased<rhs_level, Value, rhs_intrsp> const& rhs)
{
    if constexpr (lhs_level != rhs_level)
    {
        return false;
    }
    return lhs.equals(rhs);
}

template<
    caching_level_type lhs_level,
    caching_level_type rhs_level,
    typename Value,
    bool lhs_intrsp,
    bool rhs_intrsp>
bool
operator<(
    function_request_erased<lhs_level, Value, lhs_intrsp> const& lhs,
    function_request_erased<rhs_level, Value, rhs_intrsp> const& rhs)
{
    if constexpr (lhs_level != rhs_level)
    {
        return lhs_level < rhs_level;
    }
    return lhs.less_than(rhs);
}

template<caching_level_type level, typename Value, bool intrsp>
size_t
hash_value(function_request_erased<level, Value, intrsp> const& req)
{
    return req.hash();
}

template<caching_level_type level, typename Value, bool intrsp>
void
update_unique_hash(
    unique_hasher& hasher,
    function_request_erased<level, Value, intrsp> const& req)
{
    req.update_hash(hasher);
}

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
auto
rq_function_erased(Function function, Args... args)
{
    using value_type = std::
        invoke_result_t<Function, typename Args::element_type::value_type...>;
    return function_request_erased<level, value_type, false>{
        std::move(function), std::move(args)...};
}

template<caching_level_type level, class Function, class... Args>
auto
rq_function_erased_intrsp(
    std::string const& title, Function function, Args... args)
{
    using value_type = std::
        invoke_result_t<Function, typename Args::element_type::value_type...>;
    return function_request_erased<level, value_type, true>{
        title, std::move(function), std::move(args)...};
}

template<caching_level_type level, class Function, class... Args>
auto
rq_function_erased_intrsp(std::string&& title, Function function, Args... args)
{
    using value_type = std::
        invoke_result_t<Function, typename Args::element_type::value_type...>;
    return function_request_erased<level, value_type, true>{
        std::move(title), std::move(function), std::move(args)...};
}

} // namespace cradle

#endif
