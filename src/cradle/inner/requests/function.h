#ifndef CRADLE_INNER_REQUESTS_FUNCTION_H
#define CRADLE_INNER_REQUESTS_FUNCTION_H

#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include <cereal/types/memory.hpp>
#include <cereal/types/tuple.hpp>
#include <cppcoro/schedule_on.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>
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
 *
 * This file defines the "type-erased" requests.
 * The main request object (class function_request_erased) has a shared_ptr
 * to a function_request_intf object; that object's full type
 * (i.e., function_request_impl's template arguments) are known in
 * function_request_erased's constructor only.
 *
 * These classes intend to overcome the drawbacks of the earlier ones
 * (function_deprecated.h).
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

// Visits a request's argument if it's not a subrequest.
template<typename Val>
void
visit_arg(req_visitor_intf& visitor, std::size_t ix, Val const& val)
{
    visitor.visit_val_arg(ix);
}

// Visits a subrequest, and recursively visits its arguments.
// TODO seems useful only if req's function is a coro. Not for value_request
template<Request Req>
void
visit_arg(req_visitor_intf& visitor, std::size_t ix, Req const& req)
{
    auto sub_visitor = visitor.visit_req_arg(ix);
    req.visit(*sub_visitor);
}

// Recursively visits all arguments of a request, and its subrequests.
template<typename Args, std::size_t... Ix>
auto
visit_args(
    req_visitor_intf& visitor, Args const& args, std::index_sequence<Ix...>)
{
    (visit_arg(visitor, Ix, std::get<Ix>(args)), ...);
}

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

    virtual void
    visit(req_visitor_intf& visitor) const = 0;

    virtual cppcoro::task<Value>
    resolve(Ctx& ctx) const = 0;
};

template<typename Ctx, typename Args, std::size_t... Ix>
auto
make_sync_sub_tasks(Ctx& ctx, Args const& args, std::index_sequence<Ix...>)
{
    return std::make_tuple(resolve_request(ctx, std::get<Ix>(args))...);
}

template<typename Ctx, typename Args, std::size_t... Ix>
auto
make_async_sub_tasks(Ctx& ctx, Args const& args, std::index_sequence<Ix...>)
{
    return std::make_tuple(
        resolve_request(ctx.get_sub(Ix), std::get<Ix>(args))...);
}

template<typename Tuple, std::size_t... Ix>
auto
when_all_wrapper(Tuple&& tasks, std::index_sequence<Ix...>)
{
    return cppcoro::when_all(std::get<Ix>(std::forward<Tuple>(tasks))...);
}

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
    using this_type = function_request_impl;
    using base_type = Intf;
    using context_type = typename Intf::context_type;

    static constexpr bool func_is_coro = AsCoro;
    static constexpr bool func_is_plain
        = std::is_function_v<std::remove_pointer_t<Function>>;
    static constexpr bool is_async = LocalAsyncContext<context_type>;

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

    void
    visit(req_visitor_intf& visitor) const override
    {
        using Indices = std::index_sequence_for<Args...>;
        visit_args(visitor, args_, Indices{});
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
        else if constexpr (!is_async)
        {
            using Indices = std::index_sequence_for<Args...>;
            auto sub_tasks = make_sync_sub_tasks(ctx, args_, Indices{});
            co_return co_await std::apply(
                *function_,
                std::tuple_cat(
                    std::tie(ctx),
                    co_await when_all_wrapper(
                        std::move(sub_tasks), Indices{})));
        }
        else
        {
            // Throws on error or cancellation.
            // If a subtask throws (because of cancellation or otherwise),
            // the main task will wait until all other subtasks have finished
            // (or thrown).
            // This justifies passing contexts around by reference.
            try
            {
                using Indices = std::index_sequence_for<Args...>;
                auto sub_tasks = make_async_sub_tasks(ctx, args_, Indices{});
                ctx.update_status(async_status::SUBS_RUNNING);
                auto sub_results = co_await when_all_wrapper(
                    std::move(sub_tasks), Indices{});
                ctx.update_status(async_status::SELF_RUNNING);
                // Rescheduling allows tasks to run in parallel.
                // However, for simple tasks (e.g. identity_coro) it probably
                // isn't any good.
                // TODO make schedule() call conditional
                co_await ctx.get_thread_pool().schedule();
                auto result = co_await std::apply(
                    *function_,
                    std::tuple_cat(std::tie(ctx), std::move(sub_results)));
                ctx.update_status(async_status::FINISHED);
                co_return result;
            }
            catch (async_cancelled const&)
            {
                ctx.update_status(async_status::CANCELLED);
                throw;
            }
            catch (std::exception const& e)
            {
                ctx.update_status_error(e.what());
                throw;
            }
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
    using element_type = function_request_erased;
    using value_type = Value;
    using ctx_type = typename Props::ctx_type;
    using intf_type = function_request_intf<ctx_type, Value>;
    using props_type = Props;

    static constexpr caching_level_type caching_level = Props::level;
    static constexpr bool introspective = Props::introspective;

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

    void
    visit(req_visitor_intf& visitor) const
    {
        impl_->visit(visitor);
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

    // Used for creating placeholder subrequests in the catalog.
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
template<typename Props, typename Function, typename... Args>
requires(Props::func_is_coro) auto rq_function_erased(
    Props props, Function function, Args... args)
{
    using CtxRef = std::add_lvalue_reference_t<typename Props::ctx_type>;
    // Rely on the coroutine returning cppcoro::task<Value>,
    // which is a class that has a value_type member
    using Value = typename std::
        invoke_result_t<Function, CtxRef, arg_type<Args>...>::value_type;
    return function_request_erased<Value, Props>{
        std::move(props), std::move(function), std::move(args)...};
}

/*
 * Template arguments
 * ==================
 *
 * An argument to a function_request_erased object corresponds to some type,
 * e.g. std::string or blob. The option of having the argument be some kind
 * of subrequest will often be a requirement; in addition, the option of it
 * being a simple value would often be convenient.
 *
 * The major problem with allowing both is that they lead to different types
 * of the main function_request_erased template class. Each variant needs its
 * own uuid, and must be registered separately. If several arguments can
 * have a template type, the number of combinations quickly becomes
 * unmanageable.
 *
 * The solution to this problem is that a template argument nominally is a
 * function_request_erased object itself. It may also be a plain value, in
 * which case the framework will convert it to an internal
 * function_request_erased object that simply returns that value. The end
 * result is that the argument always is a function_request_erased object,
 * and there is just a single main function_request_erased type.
 *
 * Support for this solution consists of two parts:
 * - A TypedArg concept that checks whether a given argument is suitable
 *   for this mechanism.
 * - A set of normalize_arg() functions that convert an argument to the
 *   normalized function_request_erased form.
 */

// Checks that Arg is a value of type ValueType, or a request resolving to that
// type.
template<typename Arg, typename ValueType>
concept TypedArg
    = std::convertible_to<
          Arg,
          ValueType> || (std::same_as<typename Arg::value_type, ValueType> && std::same_as<function_request_erased<ValueType, typename Arg::props_type>, Arg>);

// Function returning the given value as-is; similar to std::identity.
template<typename Value>
Value
identity_func(Value&& value)
{
    return std::forward<Value>(value);
}

// Coroutine returning the given value as-is.
template<typename Value>
cppcoro::task<Value>
identity_coro(context_intf& ctx, Value value)
{
    co_return value;
}

// Contains the uuid string for a normalize_arg request. The uuid (only)
// depends on the value type that the request resolves to.
// Note: don't put the request_uuid itself in the struct as it depends
// on the static Git version which is also evaluated at C++ initialization
// time.
// TODO put specializations in separate .h?
// TODO is it possible to raise a compile-time error when instantiating
// the generic template?
template<typename Value>
struct normalization_uuid
{
};

template<>
struct normalization_uuid<int>
{
    static const inline std::string uuid_str{"normalization_uuid<int>"};
};

template<>
struct normalization_uuid<std::string>
{
    static const inline std::string uuid_str{"normalization_uuid<string>"};
};

template<>
struct normalization_uuid<blob>
{
    static const inline std::string uuid_str{"normalization_uuid<blob>"};
};

template<typename Value>
auto
make_normalization_uuid()
{
    return request_uuid{normalization_uuid<Value>::uuid_str};
}

/*
 * Converts an argument value to a rq_function_erased resolving to that value.
 * If the argument already is a rq_function_erased, it is returned as-is.
 *
 * The general normalize_arg() would look like this:
 *
 *   template<typename Value, typename Props, typename Arg>
 *   auto
 *   normalize_arg(Arg const& arg);
 *
 * Value and Props must be specified, Arg will be deduced.
 * Props is a request_props instantiation, and must equal the one for the main
 * request.
 *
 * TODO uuid must differ between different Props types.
 */

// Normalizes a value argument in a non-coroutine context.
template<typename Value, typename Props>
requires(!Request<Value> && !Props::func_is_coro) auto normalize_arg(
    Value const& arg)
{
    Props props{make_normalization_uuid<Value>(), "arg"};
    return rq_function_erased(std::move(props), identity_func<Value>, arg);
}

// Normalizes a value argument in a coroutine context.
template<typename Value, typename Props>
requires(!Request<Value> && Props::func_is_coro) auto normalize_arg(
    Value const& arg)
{
    Props props{make_normalization_uuid<Value>(), "arg"};
    return rq_function_erased(std::move(props), identity_coro<Value>, arg);
}

// Normalizes a C-style string argument in a non-coroutine context.
template<typename Value, typename Props>
requires(
    std::same_as<
        Value,
        std::string> && !Props::func_is_coro) auto normalize_arg(char const*
                                                                     arg)
{
    Props props{make_normalization_uuid<Value>(), "arg"};
    return rq_function_erased(
        std::move(props), identity_func<std::string>, std::string{arg});
}

// Normalizes a C-style string argument in a coroutine context.
template<typename Value, typename Props>
requires(std::same_as<Value, std::string>&&
             Props::func_is_coro) auto normalize_arg(char const* arg)
{
    Props props{make_normalization_uuid<Value>(), "arg"};
    return rq_function_erased(
        std::move(props), identity_coro<std::string>, std::string{arg});
}

// Normalizes a function_request_erased argument (returned as-is).
// If a subrequest is passed as argument, its Props must equal those for the
// main request.
template<typename Value, typename Props, typename Arg>
requires std::same_as<function_request_erased<Value, Props>, Arg> auto
normalize_arg(Arg const& arg)
{
    return arg;
}

} // namespace cradle

#endif
