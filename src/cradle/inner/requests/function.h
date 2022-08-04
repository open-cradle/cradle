#ifndef CRADLE_INNER_REQUESTS_FUNCTION_H
#define CRADLE_INNER_REQUESTS_FUNCTION_H

#include <cassert>
#include <memory>
#include <optional>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include <cereal/types/memory.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/id.h>
#include <cradle/inner/core/sha256_hash_id.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/core.h>

namespace cradle {

template<typename Value, typename Function, typename... Args>
class function_request_uncached
{
 public:
    using element_type = function_request_uncached;
    using value_type = Value;

    static constexpr caching_level_type caching_level
        = caching_level_type::none;

    function_request_uncached(Function function, Args... args)
        : function_{std::move(function)}, args_{std::move(args)...}
    {
    }

    template<UncachedContext Ctx>
    cppcoro::task<Value>
    resolve(Ctx& ctx) const
    {
        // The "func=function_" is a workaround to prevent a gcc-10 internal
        // compiler error in release builds.
        co_return co_await std::apply(
            [&, func = function_](auto&&... args)
                -> cppcoro::task<Value> {
                co_return func((co_await resolve_request(ctx, args))...);
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

// TODO does function_request_cached have any advantages over
// function_request_erased?
// - args_ are duplicated in id_
// - id_ contains a shared_ptr
// TODO cannot disk cache because Function cannot be part of a persistent
// identity
template<
    caching_level_type level,
    typename Value,
    typename Function,
    typename... Args>
class function_request_cached
{
 public:
    using element_type = function_request_cached;
    using value_type = Value;

    static constexpr caching_level_type caching_level = level;
    static constexpr bool introspective = false;

    function_request_cached(Function function, Args... args)
        : id_{make_captured_sha256_hashed_id(
            typeid(function).name(), args...)},
          function_{std::move(function)},
          args_{std::move(args)...}
    {
    }

    template<
        caching_level_type level1,
        typename Value1,
        typename Function1,
        typename... Args1>
    bool
    equals(function_request_cached<level1, Value1, Function1, Args1...> const&
               other) const
    {
        if constexpr (level != level1)
        {
            return false;
        }
        return *id_ == *other.id_;
    }

    template<
        caching_level_type level1,
        typename Value1,
        typename Function1,
        typename... Args1>
    bool
    less_than(
        function_request_cached<level1, Value1, Function1, Args1...> const&
            other) const
    {
        if constexpr (level != level1)
        {
            return level < level1;
        }
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

    captured_id const&
    get_captured_id() const
    {
        return id_;
    }

    template<CachedContext Ctx>
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
    caching_level_type level,
    typename Value,
    typename Function,
    typename... Args>
struct arg_type_struct<
    function_request_cached<level, Value, Function, Args...>>
{
    using value_type = Value;
};

template<
    caching_level_type level,
    typename Value,
    typename Function,
    typename... Args>
struct arg_type_struct<
    std::unique_ptr<function_request_cached<level, Value, Function, Args...>>>
{
    using value_type = Value;
};

template<
    caching_level_type level,
    typename Value,
    typename Function,
    typename... Args>
struct arg_type_struct<
    std::shared_ptr<function_request_cached<level, Value, Function, Args...>>>
{
    using value_type = Value;
};

template<
    caching_level_type level0,
    typename Value0,
    typename Function0,
    typename... Args0,
    caching_level_type level1,
    typename Value1,
    typename Function1,
    typename... Args1>
bool
operator==(
    function_request_cached<level0, Value0, Function0, Args0...> const& lhs,
    function_request_cached<level1, Value1, Function1, Args1...> const& rhs)
{
    return lhs.equals(rhs);
}

template<
    caching_level_type level0,
    typename Value0,
    typename Function0,
    typename... Args0,
    caching_level_type level1,
    typename Value1,
    typename Function1,
    typename... Args1>
bool
operator<(
    function_request_cached<level0, Value0, Function0, Args0...> const& lhs,
    function_request_cached<level1, Value1, Function1, Args1...> const& rhs)
{
    return lhs.less_than(rhs);
}

template<
    caching_level_type level,
    typename Value,
    typename Function,
    typename... Args>
size_t
hash_value(function_request_cached<level, Value, Function, Args...> const& req)
{
    return req.hash();
}

template<
    caching_level_type level,
    typename Value,
    typename Function,
    typename... Args>
void
update_unique_hash(
    unique_hasher& hasher,
    function_request_cached<level, Value, Function, Args...> const& req)
{
    req.update_hash(hasher);
}

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
// TODO don't we need a serialize() member function?
template<typename Value, typename Function, typename... Args>
class function_request_impl : public function_request_intf<Value>
{
 public:
    // uuid is allowed to be empty if caching level is not full
    function_request_impl(std::string uuid, Function function, Args... args)
        : uuid_{std::move(uuid)},
          function_{std::move(function)},
          args_{std::move(args)...}
    {
    }

    bool
    equals(function_request_impl const& other) const
    {
        if (this == &other)
        {
            return true;
        }
        return get_function_type_index() == other.get_function_type_index()
               && args_ == other.args_;
    }

    bool
    less_than(function_request_impl const& other) const
    {
        if (this == &other)
        {
            return false;
        }
        auto this_type_index = get_function_type_index();
        auto other_type_index = other.get_function_type_index();
        if (this_type_index != other_type_index)
        {
            return this_type_index < other_type_index;
        }
        return args_ < other.args_;
    }

    bool
    equals(id_interface const& other) const override
    {
        assert(dynamic_cast<function_request_impl const*>(&other));
        auto const& other_id
            = static_cast<function_request_impl const&>(other);
        return equals(other_id);
    }

    bool
    less_than(id_interface const& other) const override
    {
        assert(dynamic_cast<function_request_impl const*>(&other));
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
            auto function_hash = invoke_hash(get_function_type_index());
            auto args_hash = std::apply(
                [](auto&&... args) {
                    return combine_hashes(invoke_hash(args)...);
                },
                args_);
            hash_ = combine_hashes(function_hash, args_hash);
        }
        return *hash_;
    }

    void
    update_hash(unique_hasher& hasher) const override
    {
        if (!have_unique_hash_)
        {
            calc_unique_hash();
        }
        hasher.combine(unique_hash_);
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
    std::string uuid_;
    Function function_;
    std::tuple<Args...> args_;
    mutable std::optional<size_t> hash_;
    mutable unique_hasher::result_t unique_hash_;
    mutable bool have_unique_hash_{false};

    std::type_index
    get_function_type_index() const
    {
        // The typeid() is evaluated at compile time
        return std::type_index(typeid(Function));
    }

    void
    calc_unique_hash() const
    {
        unique_hasher hasher;
        assert(!uuid_.empty());
        update_unique_hash(hasher, uuid_);
        std::apply(
            [&hasher](auto&&... args) {
                (update_unique_hash(hasher, args), ...);
            },
            args_);
        hasher.get_result(unique_hash_);
        have_unique_hash_ = true;
    }
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
    requires(!introspective) function_request_erased(
        std::string uuid, Function function, Args... args)
    {
        auto impl{
            std::make_shared<function_request_impl<Value, Function, Args...>>(
                std::move(uuid), std::move(function), std::move(args)...)};
        impl_ = impl;
        if constexpr (caching_level != caching_level_type::none)
        {
            captured_id_ = captured_id{impl};
        }
    }

    template<typename Function, typename... Args>
    requires(introspective) function_request_erased(
        std::string uuid, std::string title, Function function, Args... args)
        : title_{std::move(title)}
    {
        auto impl{
            std::make_shared<function_request_impl<Value, Function, Args...>>(
                std::move(uuid), std::move(function), std::move(args)...)};
        impl_ = impl;
        if constexpr (caching_level != caching_level_type::none)
        {
            captured_id_ = captured_id{impl};
        }
    }

    // TODO shouldn't this be a template function?
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
            // TODO split this class into uncached/cached ones
            // Could also pass cached_context_intf& to resolve() for cached
            throw not_implemented_error(
                "captured_id only available for cached function requests");
        }
        return captured_id_;
    }

    cppcoro::task<Value>
    resolve(context_intf& ctx) const
    {
        return impl_->resolve(ctx);
    }

    std::string
    get_introspection_title() const requires(introspective)
    {
        return title_;
    }

    template<typename Archive>
    void
    serialize(Archive& archive)
    {
        archive(impl_);
    }

 private:
    std::string title_;
    std::shared_ptr<function_request_intf<Value>> impl_;
    captured_id captured_id_;
};

template<caching_level_type level, typename Value, bool introspective_>
struct arg_type_struct<function_request_erased<level, Value, introspective_>>
{
    using value_type = Value;
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

template<caching_level_type level, typename Function, typename... Args>
requires(level == caching_level_type::none) auto rq_function(
    Function function, Args... args)
{
    using value_type = std::invoke_result_t<Function, arg_type<Args>...>;
    return function_request_uncached<value_type, Function, Args...>{
        std::move(function), std::move(args)...};
}

template<caching_level_type level, typename Function, typename... Args>
requires(level != caching_level_type::none) auto rq_function(
    Function function, Args... args)
{
    using value_type = std::invoke_result_t<Function, arg_type<Args>...>;
    return function_request_cached<level, value_type, Function, Args...>{
        std::move(function), std::move(args)...};
}

template<
    typename Value,
    caching_level_type level,
    typename Function,
    typename... Args>
requires(level != caching_level_type::none) auto rq_function(
    Function function, Args... args)
{
    return function_request_cached<level, Value, Function, Args...>{
        std::move(function), std::move(args)...};
}

template<caching_level_type level, typename Function, typename... Args>
requires(level == caching_level_type::none) auto rq_function_up(
    Function function, Args... args)
{
    using value_type = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_unique<
        function_request_uncached<value_type, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type level, typename Function, typename... Args>
requires(level != caching_level_type::none) auto rq_function_up(
    Function function, Args... args)
{
    using value_type = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_unique<
        function_request_cached<level, value_type, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type level, typename Function, typename... Args>
requires(level == caching_level_type::none) auto rq_function_sp(
    Function function, Args... args)
{
    using value_type = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_shared<
        function_request_uncached<value_type, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type level, typename Function, typename... Args>
requires(level != caching_level_type::none) auto rq_function_sp(
    Function function, Args... args)
{
    using value_type = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_shared<
        function_request_cached<level, value_type, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type level, typename Function, typename... Args>
requires(level != caching_level_type::full) auto rq_function_erased(
    Function function, Args... args)
{
    using value_type = std::invoke_result_t<Function, arg_type<Args>...>;
    return function_request_erased<level, value_type, false>{
        "", std::move(function), std::move(args)...};
}

template<caching_level_type level, typename Function, typename... Args>
auto
rq_function_erased_uuid(std::string uuid, Function function, Args... args)
{
    using value_type = std::invoke_result_t<Function, arg_type<Args>...>;
    assert(!uuid.empty());
    return function_request_erased<level, value_type, false>{
        std::move(uuid), std::move(function), std::move(args)...};
}

template<caching_level_type level, typename Function, typename... Args>
requires(level != caching_level_type::full) auto rq_function_erased_intrsp(
    std::string title, Function function, Args... args)
{
    using value_type = std::invoke_result_t<Function, arg_type<Args>...>;
    return function_request_erased<level, value_type, true>{
        "", std::move(title), std::move(function), std::move(args)...};
}

template<caching_level_type level, typename Function, typename... Args>
auto
rq_function_erased_uuid_intrsp(
    std::string uuid, std::string title, Function function, Args... args)
{
    using value_type = std::invoke_result_t<Function, arg_type<Args>...>;
    assert(!uuid.empty());
    return function_request_erased<level, value_type, true>{
        std::move(uuid),
        std::move(title),
        std::move(function),
        std::move(args)...};
}

} // namespace cradle

#endif
