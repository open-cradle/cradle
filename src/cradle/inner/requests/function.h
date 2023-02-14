#ifndef CRADLE_INNER_REQUESTS_FUNCTION_H
#define CRADLE_INNER_REQUESTS_FUNCTION_H

#include <cassert>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include <cereal/types/memory.hpp>
#include <cereal/types/tuple.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/id.h>
#include <cradle/inner/core/sha256_hash_id.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/encodings/cereal.h>
#include <cradle/inner/requests/cereal.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/service/request.h>

namespace cradle {

/*
 * Requests based on a function, which can be either a "normal" function
 * (plain C++ function or object), or a coroutine.
 * Currently, a coroutine takes a context as its first argument, whereas
 * a normal function does not. This could be split into four cases
 * (function/coroutine with/without context argument).
 */

class conflicting_functions_uuid_error : public uuid_error
{
 public:
    using uuid_error::uuid_error;
};

class no_function_for_uuid_error : public uuid_error
{
 public:
    using uuid_error::uuid_error;
};

class missing_uuid_error : public uuid_error
{
 public:
    using uuid_error::uuid_error;
};

/*
 * The first part of this file defines the "original" function requests,
 * that have no type erasure.
 *
 * They do have some drawbacks:
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
 * So normally the type-erased requests (below) should be preferred.
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
    requires(Level != caching_level_type::full)
class function_request_cached
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
    requires(Level == caching_level_type::none)
auto rq_function(Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return function_request_uncached<Value, Function, Args...>{
        std::move(function), std::move(args)...};
}

template<caching_level_type Level, typename Function, typename... Args>
    requires(Level != caching_level_type::none)
auto rq_function(Function function, Args... args)
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
    requires(Level != caching_level_type::none)
auto rq_function(Function function, Args... args)
{
    return function_request_cached<Level, Value, Function, Args...>{
        std::move(function), std::move(args)...};
}

template<caching_level_type Level, typename Function, typename... Args>
    requires(Level == caching_level_type::none)
auto rq_function_up(Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_unique<
        function_request_uncached<Value, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type Level, typename Function, typename... Args>
    requires(Level != caching_level_type::none)
auto rq_function_up(Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_unique<
        function_request_cached<Level, Value, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type Level, typename Function, typename... Args>
    requires(Level == caching_level_type::none)
auto rq_function_sp(Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_shared<
        function_request_uncached<Value, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type Level, typename Function, typename... Args>
    requires(Level != caching_level_type::none)
auto rq_function_sp(Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return std::make_shared<
        function_request_cached<Level, Value, Function, Args...>>(
        std::move(function), std::move(args)...);
}

/*
 * The second part of this file defines the "type-erased" requests.
 * The main request object (class function_request_erased) has a shared_ptr
 * to a function_request_intf object; that object's full type
 * (i.e., function_request_impl's template arguments) are known in
 * function_request_erased's constructor only.
 *
 * These classes intend to overcome the drawbacks of the earlier ones.
 */

/*
 * The interface type exposing the functionality that function_request_erased
 * requires outside its constructor.
 *
 * Ctx is the "minimum" context needed to resolve this request.
 * E.g. a "cached" context can be used to resolve a non-cached request.
 */
template<Context Ctx, typename Value>
class function_request_intf : public id_interface
{
 public:
    using context_type = Ctx;

    virtual ~function_request_intf() = default;

    virtual request_uuid
    get_uuid() const
        = 0;

    virtual cppcoro::task<Value>
    resolve(Ctx& ctx) const = 0;
};

/*
 * The actual type created by function_request_erased, but visible only in its
 * constructor (and erased elsewhere).
 *
 * Intf is a function_request_intf instantiation.
 * Function must be MoveAssignable but may not be CopyAssignable (e.g. if it's
 * a lambda).
 *
 * Only a small part of this class depends on the context type, so there will
 * be object code duplication if multiple instantiations exist differing in
 * the context (i.e., introspective + caching level) only. Maybe this could be
 * optimized if it becomes an issue.
 *
 * If Function is a class, then the type of a function_request_impl
 * instantiation will uniquely identify it.
 * If Function is a C++ function, then the type of a function_request_impl
 * instantiation will correspond to all C++ functions having the same
 * signature, so it must be combined with the function's address to achieve
 * that uniqueness.
 * This uniqeness is relevant when deserializing a type-erased request. Its
 * uuid will identify both the function_request_impl class, and (if
 * relevant) the C++ function. This implies a two-step process: first a
 * function_request_impl object is created of the specified class, then that
 * object's function member is set to the correct (pointer) value.
 */
template<
    typename Value,
    typename Intf,
    bool AsCoro,
    typename Function,
    typename... Args>
class function_request_impl : public Intf
{
 public:
    static constexpr bool func_is_coro = AsCoro;
    static constexpr bool func_is_plain
        = std::is_function_v<std::remove_pointer_t<Function>>;
    using this_type = function_request_impl;
    using base_type = Intf;
    using context_type = typename Intf::context_type;

    function_request_impl(request_uuid uuid, Function function, Args... args)
        : uuid_{std::move(uuid)}, args_{std::move(args)...}
    {
        // Guaranteed by the function_request_erased ctor
        assert(uuid_.is_real());

        // The uuid uniquely identifies the function.
        // Have a single shared_ptr per function
        // (though not really necessary).
        auto uuid_str{uuid_.str()};
        std::scoped_lock lock{static_mutex_};
        auto it = matching_functions_.find(uuid_str);
        if (it == matching_functions_.end())
        {
            register_polymorphic_type<this_type, base_type>(uuid_);
            function_ = std::make_shared<Function>(std::move(function));
            matching_functions_[uuid_str] = function_;
        }
        else
        {
            function_ = matching_functions_[uuid_str];
            // Attempts to associate more than one Function (type) with
            // a single uuid will be caught in uuid_registry.
            // The following check catches attempts to associate more than
            // one C++ function with a single uuid.
            if constexpr (func_is_plain)
            {
                if (**function_ != *function)
                {
                    throw conflicting_functions_uuid_error(fmt::format(
                        "Multiple C++ functions for uuid {}", uuid_str));
                }
            }
        }
    }

    // other will be a function_request_impl, but possibly instantiated from
    // different template arguments.
    bool
    equals(id_interface const& other) const override
    {
        if (auto other_impl
            = dynamic_cast<function_request_impl const*>(&other))
        {
            return equals_same_type(*other_impl);
        }
        return false;
    }

    // other will be a function_request_impl, but possibly instantiated from
    // different template arguments.
    bool
    less_than(id_interface const& other) const override
    {
        if (auto other_impl
            = dynamic_cast<function_request_impl const*>(&other))
        {
            return less_than_same_type(*other_impl);
        }
        return typeid(*this).before(typeid(other));
    }

    // Maybe caching the hashes could be optional (policy?).
    size_t
    hash() const override
    {
        if (!hash_)
        {
            auto function_type_hash = invoke_hash(get_function_type_index());
            auto args_hash = std::apply(
                [](auto&&... args) {
                    return combine_hashes(invoke_hash(args)...);
                },
                args_);
            if constexpr (func_is_plain)
            {
                auto function_hash = invoke_hash(*function_);
                hash_ = combine_hashes(
                    function_type_hash, function_hash, args_hash);
            }
            else
            {
                hash_ = combine_hashes(function_type_hash, args_hash);
            }
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

    request_uuid
    get_uuid() const override
    {
        return uuid_;
    }

    cppcoro::task<Value>
    resolve(context_type& ctx) const override
    {
        // If there is no coroutine function and no caching in the request
        // tree, there is nothing to co_await on (but how useful would such
        // a request be?).
        // TODO consider optimizing resolve() for "simple" request trees
        // The std::forward probably doesn't help as all resolve_request()
        // variants take the arg as const&.
        if constexpr (!func_is_coro)
        {
            // gcc tends to have trouble with this piece of code (leading to
            // compiler crashes or runtime errors).
            co_return co_await std::apply(
                [&](auto&&... args) -> cppcoro::task<Value> {
                    co_return (*function_)((co_await resolve_request(
                        ctx, std::forward<decltype(args)>(args)))...);
                },
                args_);
        }
        else
        {
            co_return co_await std::apply(
                [&](auto&&... args) -> cppcoro::task<Value> {
                    co_return co_await (*function_)(
                        ctx,
                        (co_await resolve_request(
                            ctx, std::forward<decltype(args)>(args)))...);
                },
                args_);
        }
    }

 private:
    // cereal-related

    friend class cereal::access;
    function_request_impl()
    {
    }

 public:
    template<typename Archive>
    void
    save(Archive& archive) const
    {
        // Trust archive to be save-only
        const_cast<function_request_impl*>(this)->load_save(archive);
    }

    template<typename Archive>
    void
    load(Archive& archive)
    {
        load_save(archive);
        std::scoped_lock lock{static_mutex_};
        auto it = matching_functions_.find(uuid_.str());
        if (it == matching_functions_.end())
        {
            // This cannot happen.
            throw no_function_for_uuid_error(
                fmt::format("No function found for uuid {}", uuid_.str()));
        }
        function_ = it->second;
    }

 private:
    // Adding the make_nvp's allows changing the order of uuid and args
    // in the JSON.
    template<typename Archive>
    void
    load_save(Archive& archive)
    {
        archive(
            cereal::make_nvp("uuid", uuid_), cereal::make_nvp("args", args_));
    }

 private:
    // Protector of matching_functions_
    static inline std::mutex static_mutex_;

    // The functions matching this request's type, indexed by uuid string.
    // Used only when the uuid is serializable.
    // If Function is a class, the map size will normally be one (unless
    // multiple uuid's refer to the same function), and all map values will
    // be equal.
    // The function cannot be serialized, but somehow needs to be set when
    // deserializing, if possible in a type-safe way. This is achieved by
    // registering an object of this class: its function will be put in this
    // map.
    static inline std::unordered_map<std::string, std::shared_ptr<Function>>
        matching_functions_;

    // If serializable, uniquely identifies the function.
    request_uuid uuid_;

    // The function to call when the request is resolved. If the uuid is
    // serializable, function_ will be one of matching_functions_'s values.
    std::shared_ptr<Function> function_;

    // The arguments to pass to the function.
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

    // *this and other are the same type, so their function types (i.e.,
    // typeid(Function)) are identical. The functions themselves might still
    // differ if they are plain C++ functions.
    // Likewise, argument types will be identical, but their values might
    // differ.
    bool
    equals_same_type(function_request_impl const& other) const
    {
        if (this == &other)
        {
            return true;
        }
        if constexpr (func_is_plain)
        {
            if (**function_ != **other.function_)
            {
                return false;
            }
        }
        return args_ == other.args_;
    }

    // *this and other are the same type.
    bool
    less_than_same_type(function_request_impl const& other) const
    {
        if (this == &other)
        {
            return false;
        }
        if constexpr (func_is_plain)
        {
            if (**function_ != **other.function_)
            {
                // Clang refuses to compare using <
                return std::less<Function>{}(*function_, *other.function_);
            }
        }
        return args_ < other.args_;
    }

    void
    calc_unique_hash() const
    {
        unique_hasher hasher;
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

// Request properties that would be identical between similar requests
template<
    caching_level_type Level,
    bool AsCoro = false,
    bool Introspective = false,
    typename Ctx = ctx_type_for_props<Introspective, Level>>
struct request_props
{
    static constexpr caching_level_type level = Level;
    static constexpr bool func_is_coro = AsCoro;
    static constexpr bool introspective = Introspective;
    using ctx_type = Ctx;

    request_uuid uuid_;
    std::string title_; // Used only if introspective

    request_props(
        request_uuid uuid = request_uuid(), std::string title = std::string())
        : uuid_(std::move(uuid)), title_(std::move(title))
    {
        assert(!(introspective && title_.empty()));
    }
};

/*
 * A function request that erases function and arguments types
 *
 * This class supports two kinds of functions:
 * (0) Plain function: res = function(args...)
 * (1) Coroutine needing context: res = co_await function(ctx, args...)
 *
 * Props::introspective is a template argument instead of being passed by value
 * because of the overhead, in object size and execution time, when resolving
 * an introspective request; see resolve_request_cached().
 *
 * When calculating the disk cache key (unique hash) for a type-erased
 * function, the key should depend on the erased type; this is achieved by
 * letting the request have a uuid. This uuid will also identify the type
 * of non-type-erased arguments appearing in the request tree, but it cannot
 * discriminate between e.g. two type-erased subrequests differing in their
 * functor only. This means that these subrequests should also have a uuid,
 * even if they themselves are not disk-cached.
 *
 * Conclusion: a type-erased request must have a uuid when its own caching
 * level is disk-cached, or it could be used as a (sub-)argument of a
 * type-erased request. The most practical solution is to require that _all_
 * type-erased requests have a uuid.
 */
template<typename Value, typename Props>
class function_request_erased
{
 public:
    static constexpr caching_level_type caching_level = Props::level;
    static constexpr bool introspective = Props::introspective;

    using element_type = function_request_erased;
    using value_type = Value;
    using ctx_type = typename Props::ctx_type;
    using intf_type = function_request_intf<ctx_type, Value>;
    using props_type = Props;

    template<typename Function, typename... Args>
    function_request_erased(Props props, Function function, Args... args)
        : title_{std::move(props.title_)}
    {
        // TODO make is_real() a compile-time thing
        if (!props.uuid_.is_real())
        {
            throw missing_uuid_error{
                "Real uuid needed for type-erased request"};
        }
        using impl_type = function_request_impl<
            Value,
            intf_type,
            Props::func_is_coro,
            Function,
            Args...>;
        impl_ = std::make_shared<impl_type>(
            std::move(props.uuid_), std::move(function), std::move(args)...);
        init_captured_id();
    }

    // *this and other are the same type; however, their impl_'s types could
    // differ (especially if these are subrequests).
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
        return impl_->hash();
    }

    void
    update_hash(unique_hasher& hasher) const
    {
        impl_->update_hash(hasher);
    }

    captured_id const&
    get_captured_id() const
        requires(caching_level != caching_level_type::none)
    {
        return captured_id_;
    }

    request_uuid
    get_uuid() const
    {
        return impl_->get_uuid();
    }

    template<typename Ctx>
    requires ContextMatchingProps<Ctx, introspective, caching_level>
        cppcoro::task<Value>
        resolve(Ctx& ctx) const
    {
        return impl_->resolve(ctx);
    }

    std::string
    get_introspection_title() const
        requires(introspective)
    {
        return title_;
    }

 public:
    // Interface for cereal
    // Also for creating placeholder subrequests in the catalog.
    function_request_erased() = default;

    // Construct object, deserializing from a cereal archive
    // Convenience constructor for when this is the "outer" object.
    // Equivalent alternative:
    //   function_request_erased<...> req;
    //   req.load(archive)
    template<typename Archive>
    explicit function_request_erased(Archive& archive)
    {
        load(archive);
    }

    template<typename Archive>
    void
    save(Archive& archive) const
    {
        archive(
            cereal::make_nvp("impl", impl_),
            cereal::make_nvp("title", title_));
    }

    template<typename Archive>
    void
    load(Archive& archive)
    {
        archive(impl_, title_);
        init_captured_id();
    }

 private:
    std::string title_;
    std::shared_ptr<intf_type> impl_;
    // captured_id_, if set, will hold a shared_ptr reference to impl_
    captured_id captured_id_;

    void
    init_captured_id()
    {
        if constexpr (caching_level != caching_level_type::none)
        {
            captured_id_ = captured_id{impl_};
        }
    }
};

template<typename Value, typename Props>
struct arg_type_struct<function_request_erased<Value, Props>>
{
    using value_type = Value;
};

// Used for comparing subrequests, where the main requests have the same type;
// so the subrequests have the same type too.
template<typename Value, typename Props>
bool
operator==(
    function_request_erased<Value, Props> const& lhs,
    function_request_erased<Value, Props> const& rhs)
{
    return lhs.equals(rhs);
}

template<typename Value, typename Props>
bool
operator<(
    function_request_erased<Value, Props> const& lhs,
    function_request_erased<Value, Props> const& rhs)
{
    return lhs.less_than(rhs);
}

template<typename Value, typename Props>
size_t
hash_value(function_request_erased<Value, Props> const& req)
{
    return req.hash();
}

template<typename Value, typename Props>
void
update_unique_hash(
    unique_hasher& hasher, function_request_erased<Value, Props> const& req)
{
    req.update_hash(hasher);
}

// Creates a type-erased request for a non-coroutine function
template<typename Props, typename Function, typename... Args>
    requires(!Props::func_is_coro)
auto rq_function_erased(Props props, Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return function_request_erased<Value, Props>{
        std::move(props), std::move(function), std::move(args)...};
}

// Creates a type-erased request for a function that is a coroutine.
// Value cannot be derived in this case so must be explicitly specified.
template<typename Value, typename Props, typename Function, typename... Args>
    requires(Props::func_is_coro)
auto rq_function_erased_coro(Props props, Function function, Args... args)
{
    return function_request_erased<Value, Props>{
        std::move(props), std::move(function), std::move(args)...};
}

} // namespace cradle

#endif
