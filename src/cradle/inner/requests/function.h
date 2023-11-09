#ifndef CRADLE_INNER_REQUESTS_FUNCTION_H
#define CRADLE_INNER_REQUESTS_FUNCTION_H

#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include <cereal/types/memory.hpp>
#include <cereal/types/tuple.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>
#include <fmt/format.h>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/id.h>
#include <cradle/inner/core/sha256_hash_id.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/encodings/cereal.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/normalization_uuid.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/requests/types.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/resolve/seri_resolver.h>

namespace cradle {

/*
 * Requests based on a function, which can be either a "normal" function
 * (plain C++ function or object), or a coroutine.
 * Currently, a coroutine takes a context as its first argument, whereas
 * a normal function does not. This could be split into four cases
 * (function/coroutine with/without context argument).
 *
 * The request classes defined in this file are "type-erased":
 * the main request object (class function_request_erased) has a shared_ptr
 * to a function_request_intf object; that object's full type
 * (i.e., function_request_impl's template arguments) are known in
 * function_request_erased's constructor only.
 */

// Visits a request's argument if it's not a subrequest.
template<typename Val>
void
visit_arg(req_visitor_intf& visitor, std::size_t ix, Val const& val)
{
    visitor.visit_val_arg(ix);
}

// Visits a request's subrequest argument, and recursively visits that
// subrequest's arguments.
template<Request SubReq>
void
visit_arg(req_visitor_intf& visitor, std::size_t ix, SubReq const& sub_req)
{
    static_assert(VisitableRequest<SubReq>);
    auto sub_visitor = visitor.visit_req_arg(ix);
    sub_req.accept(*sub_visitor);
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
 * The context references taken by resolve_sync() and resolve_async() are the
 * "minimal" ones needed.
 */
template<typename Value>
class function_request_intf : public id_interface
{
 public:
    virtual ~function_request_intf() = default;

    virtual request_uuid const&
    get_uuid() const
        = 0;

    virtual void
    register_uuid(
        seri_registry& registry,
        catalog_id cat_id,
        std::shared_ptr<seri_resolver_intf> resolver) const
        = 0;

    virtual void
    save(JSONRequestOutputArchive& archive) const
        = 0;

    virtual void
    load(JSONRequestInputArchive& archive)
        = 0;

    virtual void
    accept(req_visitor_intf& visitor) const
        = 0;

    virtual cppcoro::task<Value>
    resolve_sync(local_context_intf& ctx) const = 0;

    virtual cppcoro::task<Value>
    resolve_async(local_async_context_intf& ctx) const = 0;
};

template<typename Ctx, typename Args, std::size_t... Ix>
auto
make_sync_sub_tasks(Ctx& ctx, Args const& args, std::index_sequence<Ix...>)
{
    ResolutionConstraintsLocalSync constraints;
    return std::make_tuple(
        resolve_request(ctx, std::get<Ix>(args), constraints)...);
}

template<typename Ctx, typename Args, std::size_t... Ix>
auto
make_async_sub_tasks(Ctx& ctx, Args const& args, std::index_sequence<Ix...>)
{
    ResolutionConstraintsLocalAsync constraints;
    return std::make_tuple(resolve_request(
        ctx.get_local_sub(Ix), std::get<Ix>(args), constraints)...);
}

template<typename Tuple, std::size_t... Ix>
auto
when_all_wrapper(Tuple&& tasks, std::index_sequence<Ix...>)
{
    return cppcoro::when_all(std::get<Ix>(std::forward<Tuple>(tasks))...);
}

template<typename Head, typename... Tail>
struct first_element
{
    using type = Head;
};

// This function should be a no-op unless the argument results from a
// normalize_arg() call.
template<typename Arg>
void
register_uuid_for_normalized_arg(
    seri_registry& registry, catalog_id cat_id, Arg const& arg)
{
}

template<typename Args, std::size_t... Ix>
void
register_uuid_for_normalized_args(
    seri_registry& registry,
    catalog_id cat_id,
    Args const& args,
    std::index_sequence<Ix...>)
{
    // gcc says
    // error: parameter ‘cat_id’ set but not used
    // [-Werror=unused-but-set-parameter]
    // Why?
    (void) cat_id;
    (register_uuid_for_normalized_arg(registry, cat_id, std::get<Ix>(args)),
     ...);
}

/*
 * The actual type created by function_request_erased, but visible only in its
 * constructor (and erased elsewhere).
 *
 * The function is a "C" function, a pointer to one, or a lambda(-like) object.
 * Its value is passed to the constructor, and stored in this object so
 * that the request can be resolved.
 * The value is stored as std::shared_ptr<std::decay_t<Function>>:
 * if Function is a "C" function (so if std::is_function_v<Function>),
 * a pointer must be added before it can be stored. std::decay_t does this.
 * The shared_ptr is CopyAssignable, even if Function itself is not (e.g. if
 * it's a lambda).
 *
 * A function_request_impl object has an associated uuid that identifies
 * both the function_request_impl instantiation (type), and function value.
 * It is the caller's responsibility to provide a unique uuid.
 *
 * The uuid identifying both function_request_impl class and function value
 * leads to a two-step deserialization process: first a function_request_impl
 * object is created of the specified class, then that object's function_
 * member is set to the correct value.
 *
 * No part of this class depends on the actual context type used to resolve the
 * request.
 */
template<typename Value, bool AsCoro, typename Function, typename... Args>
class function_request_impl : public function_request_intf<Value>
{
 public:
    static_assert(!std::is_reference_v<Function>);

    using this_type = function_request_impl;
    using stored_function_t = std::decay_t<Function>;
    using ArgIndices = std::index_sequence_for<Args...>;

    static constexpr bool func_is_coro = AsCoro;
    static constexpr bool func_is_plain
        = std::is_function_v<std::remove_pointer_t<Function>>;

    // The caller ensures that (ignoring const, references and function
    // pointers):
    // - CtorUuid is request_uuid,
    // - CtorFunction is Function, and
    // - CtorArgs is Args,
    // but this has to be a template function to enable perfect forwarding.
    template<typename CtorUuid, typename CtorFunction, typename... CtorArgs>
    function_request_impl(
        CtorUuid&& uuid, CtorFunction&& function, CtorArgs&&... args)
        : uuid_{std::forward<CtorUuid>(uuid)},
          function_{std::make_shared<stored_function_t>(
              std::forward<CtorFunction>(function))},
          args_{std::forward<CtorArgs>(args)...}
    {
        static_assert(
            std::is_same_v<std::remove_cvref_t<CtorUuid>, request_uuid>);
        static_assert(
            std::
                is_same_v<std::decay_t<CtorFunction>, std::decay_t<Function>>);
    }

    // Registers this instantiation to be identified by its uuid, so that the
    // deserialization will translate an input containing that same uuid to an
    // object of the same type, with the same function_ value.
    void
    register_uuid(
        seri_registry& registry,
        catalog_id cat_id,
        std::shared_ptr<seri_resolver_intf> resolver) const override
    {
        registry.add(
            cat_id, uuid_.str(), std::move(resolver), create, function_);

        // Register uuids for any args resulting from normalize_arg()
        register_uuid_for_normalized_args(
            registry, cat_id, args_, ArgIndices{});
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
            auto uuid_hash = invoke_hash(uuid_);
            auto args_hash = std::apply(
                [](auto&&... args) {
                    return combine_hashes(invoke_hash(args)...);
                },
                args_);
            hash_ = combine_hashes(uuid_hash, args_hash);
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

    request_uuid const&
    get_uuid() const override
    {
        return uuid_;
    }

    void
    accept(req_visitor_intf& visitor) const override
    {
        visit_args(visitor, args_, ArgIndices{});
    }

    cppcoro::task<Value>
    resolve_sync(local_context_intf& ctx) const override
    {
        // If there is no coroutine function and no caching in the request
        // tree, there is nothing to co_await on (but how useful would such
        // a request be?).
        // TODO consider optimizing resolve() for "simple" request trees
        // The std::forward probably doesn't help as all resolve_request()
        // variants take the arg as const&.
        if constexpr (!func_is_coro)
        {
            return resolve_sync_non_coro(ctx);
        }
        else if constexpr (sizeof...(Args) == 1)
        {
            if constexpr (std::same_as<
                              typename first_element<Args...>::type,
                              Value>)
            {
                if (is_normalizer())
                {
                    return resolve_sync_normalizer(ctx);
                }
            }
            // Object code for the following line will still be generated if
            // this is a normalizer, but it won't be called in that case.
            return resolve_sync_coro(ctx);
        }
        else
        {
            return resolve_sync_coro(ctx);
        }
    }

    cppcoro::task<Value>
    resolve_async(local_async_context_intf& ctx) const override
    {
        if constexpr (!func_is_coro)
        {
            // TODO make resolve_async() for non-coro really async
            return resolve_sync_non_coro(ctx);
        }
        else if constexpr (sizeof...(Args) == 1)
        {
            if constexpr (std::same_as<
                              typename first_element<Args...>::type,
                              Value>)
            {
                if (is_normalizer())
                {
                    return resolve_async_normalizer(ctx);
                }
            }
            return resolve_async_coro(ctx);
        }
        else
        {
            return resolve_async_coro(ctx);
        }
    }

 private:
    cppcoro::task<Value>
    resolve_sync_non_coro(local_context_intf& ctx) const
    {
        // gcc tends to have trouble with this piece of code (leading to
        // compiler crashes or runtime errors).
        ResolutionConstraintsLocalSync constraints;
        co_return co_await std::apply(
            [&](auto&&... args) -> cppcoro::task<Value> {
                co_return (*function_)((co_await resolve_request(
                    ctx, std::forward<decltype(args)>(args), constraints))...);
            },
            args_);
    }

    cppcoro::task<Value>
    resolve_sync_coro(local_context_intf& ctx) const
    {
        auto sub_tasks = make_sync_sub_tasks(ctx, args_, ArgIndices{});
        co_return co_await std::apply(
            *function_,
            std::tuple_cat(
                std::tie(ctx),
                co_await when_all_wrapper(
                    std::move(sub_tasks), ArgIndices{})));
    }

    cppcoro::task<Value>
    resolve_async_coro(local_async_context_intf& ctx) const
    {
        // Throws on error or cancellation.
        // If a subtask throws (because of cancellation or otherwise),
        // the main task will wait until all other subtasks have finished
        // (or thrown).
        // This justifies passing contexts around by reference.
        try
        {
            auto sub_tasks = make_async_sub_tasks(ctx, args_, ArgIndices{});
            ctx.update_status(async_status::SUBS_RUNNING);
            auto sub_results = co_await when_all_wrapper(
                std::move(sub_tasks), ArgIndices{});
            ctx.update_status(async_status::SELF_RUNNING);
            // Rescheduling allows tasks to run in parallel, but is not
            // always opportune
            co_await ctx.reschedule_if_opportune();
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

    // Shortcuts resolving a request generated by a normalize_arg() call:
    // just return the value that was passed to normalize_arg().
    cppcoro::task<Value>
    resolve_sync_normalizer(local_context_intf& ctx) const
    {
        co_return std::get<0>(args_);
    }

    cppcoro::task<Value>
    resolve_async_normalizer(local_async_context_intf& ctx) const
    {
        ctx.update_status(async_status::FINISHED);
        co_return std::get<0>(args_);
    }

 public:
    // cereal-related

    // Construct an object to be deserialized.
    // The uuid uniquely identifies the function. It is deserialized in
    // function_request_erased, and stored here so that is available during
    // deserialization, which populates the remainder of this object.
    function_request_impl(request_uuid&& uuid) : uuid_{std::move(uuid)}
    {
    }

    static std::shared_ptr<void>
    create(request_uuid&& uuid)
    {
        return std::make_shared<this_type>(std::move(uuid));
    }

    void
    save(JSONRequestOutputArchive& archive) const override
    {
        archive(cereal::make_nvp("args", args_));
    }

    void
    load(JSONRequestInputArchive& archive) override
    {
        auto& resources{archive.get_resources()};
        auto the_seri_registry{resources.get_seri_registry()};
        archive(cereal::make_nvp("args", args_));
        function_
            = the_seri_registry->find_function<stored_function_t>(uuid_.str());
    }

 private:
    // Uniquely identifies the function.
    request_uuid uuid_;

    // The function to call when the request is resolved.
    std::shared_ptr<stored_function_t> function_;

    // The arguments to pass to the function.
    std::tuple<std::remove_cvref_t<Args>...> args_;

    mutable std::optional<size_t> hash_;
    mutable unique_hasher::result_t unique_hash_;
    mutable bool have_unique_hash_{false};

    bool
    is_normalizer() const
    {
        return uuid_.str().starts_with("normalization<");
    }

    // *this and other are the same type, so their function types (i.e.,
    // typeid(Function)) are identical. The functions themselves might still
    // differ, in which case the uuid's should be different.
    // Likewise, argument types will be identical, but their values might
    // differ.
    bool
    equals_same_type(function_request_impl const& other) const
    {
        if (this == &other)
        {
            return true;
        }
        if (uuid_ != other.uuid_)
        {
            return false;
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
        if (uuid_ != other.uuid_)
        {
            return uuid_ < other.uuid_;
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

/*
 * An actual type created by function_request_erased, but visible only in its
 * constructor (and erased elsewhere).
 *
 * A stripped-down version of function_request_impl: it has no function and can
 * only act as proxy for remote execution.
 *
 * The uuid, in the context of this class, does not identify any function.
 *
 * Resolving this request will lead to the remote server creating a
 * corresponding function_request_impl object that _is_ able to perform the
 * real resolution.
 *
 * The request can be cached locally so this class should support comparison
 * and hashing.
 *
 * Deserializing a proxy request doesn't make sense. It cannot be registered as
 * seri resolver (thanks to the static_assert in
 * seri_catalog::register_resolver()), which means the create and load will
 * never be accessed.
 *
 * TODO consider inheriting function_request_impl from proxy_request_base
 */
template<typename Value, typename... Args>
class proxy_request_base : public function_request_intf<Value>
{
 public:
    using this_type = proxy_request_base;
    using intf_type = function_request_intf<Value>;

    template<typename CtorUuid, typename... CtorArgs>
    proxy_request_base(CtorUuid&& uuid, CtorArgs&&... args)
        : uuid_{std::forward<CtorUuid>(uuid)},
          args_{std::forward<CtorArgs>(args)...}
    {
        static_assert(
            std::is_same_v<std::remove_cvref_t<CtorUuid>, request_uuid>);
    }

    // Proxy requests should not be registered.
    void
    register_uuid(
        seri_registry& registry,
        catalog_id cat_id,
        std::shared_ptr<seri_resolver_intf> resolver) const override
    {
        throw not_implemented_error{"proxy_request_base::register_uuid"};
    }

    request_uuid const&
    get_uuid() const override
    {
        return uuid_;
    }

    // accept() is only used when locally resolving a serialized request, so
    // won't be called on this class.
    void
    accept(req_visitor_intf& visitor) const override
    {
        // TODO cheap throw from any unimplemented function
        throw not_implemented_error{"proxy_request_base::accept"};
    }

    cppcoro::task<Value>
    resolve_sync(local_context_intf& ctx) const override
    {
        throw not_implemented_error{"proxy_request_base::resolve_sync"};
    }

    cppcoro::task<Value>
    resolve_async(local_async_context_intf& ctx) const override
    {
        throw not_implemented_error{"proxy_request_base::resolve_async"};
    }

 public:
    // cereal-related

    void
    save(JSONRequestOutputArchive& archive) const override
    {
        archive(cereal::make_nvp("args", args_));
    }

    void
    load(JSONRequestInputArchive& archive) override
    {
        throw not_implemented_error{"proxy_request_base::load"};
    }

 protected:
    request_uuid uuid_;
    std::tuple<std::remove_cvref_t<Args>...> args_;
};

template<typename Value, typename... Args>
class proxy_request_uncached : public proxy_request_base<Value, Args...>
{
 public:
    using base_type = proxy_request_base<Value, Args...>;

    template<typename CtorUuid, typename... CtorArgs>
    proxy_request_uncached(CtorUuid&& uuid, CtorArgs&&... args)
        : base_type{
            std::forward<CtorUuid>(uuid), std::forward<CtorArgs>(args)...}
    {
        static_assert(
            std::is_same_v<std::remove_cvref_t<CtorUuid>, request_uuid>);
    }

    // Needed because this class ultimately derives from id_interface.
    bool
    equals(id_interface const& other) const override
    {
        assert(false);
        return false;
    }

    bool
    less_than(id_interface const& other) const override
    {
        assert(false);
        return false;
    }

    size_t
    hash() const override
    {
        assert(false);
        return 0;
    }

    void
    update_hash(unique_hasher& hasher) const override
    {
        assert(false);
    }
};

template<typename Value, typename... Args>
class proxy_request_cached : public proxy_request_base<Value, Args...>
{
 public:
    using base_type = proxy_request_base<Value, Args...>;

    template<typename CtorUuid, typename... CtorArgs>
    proxy_request_cached(CtorUuid&& uuid, CtorArgs&&... args)
        : base_type{
            std::forward<CtorUuid>(uuid), std::forward<CtorArgs>(args)...}
    {
        static_assert(
            std::is_same_v<std::remove_cvref_t<CtorUuid>, request_uuid>);
    }

    // other will be a proxy_request_cached, but possibly instantiated from
    // different template arguments.
    bool
    equals(id_interface const& other) const override
    {
        if (auto other_impl
            = dynamic_cast<proxy_request_cached const*>(&other))
        {
            return equals_same_type(*other_impl);
        }
        return false;
    }

    // other will be a proxy_request_cached, but possibly instantiated from
    // different template arguments.
    bool
    less_than(id_interface const& other) const override
    {
        if (auto other_impl
            = dynamic_cast<proxy_request_cached const*>(&other))
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
            auto uuid_hash = invoke_hash(this->uuid_);
            auto args_hash = std::apply(
                [](auto&&... args) {
                    return combine_hashes(invoke_hash(args)...);
                },
                this->args_);
            hash_ = combine_hashes(uuid_hash, args_hash);
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

 private:
    mutable std::optional<size_t> hash_;
    mutable unique_hasher::result_t unique_hash_;
    mutable bool have_unique_hash_{false};

    // *this and other are the same type. Argument types will be identical,
    // but their values might differ.
    bool
    equals_same_type(proxy_request_cached const& other) const
    {
        if (this == &other)
        {
            return true;
        }
        if (this->uuid_ != other.uuid_)
        {
            return false;
        }
        return this->args_ == other.args_;
    }

    // *this and other are the same type.
    bool
    less_than_same_type(proxy_request_cached const& other) const
    {
        if (this == &other)
        {
            return false;
        }
        if (this->uuid_ != other.uuid_)
        {
            return this->uuid_ < other.uuid_;
        }
        return this->args_ < other.args_;
    }

    void
    calc_unique_hash() const
    {
        unique_hasher hasher;
        update_unique_hash(hasher, this->uuid_);
        std::apply(
            [&hasher](auto&&... args) {
                (update_unique_hash(hasher, args), ...);
            },
            this->args_);
        hasher.get_result(unique_hash_);
        have_unique_hash_ = true;
    }
};

namespace detail {

// Holds the introspection title for a function_request_erased object, but only
// if introspection is enabled. Otherwise, there should be no impact on object
// size or generated object code.
template<bool Introspective>
class request_title_mixin
{
 protected:
    request_title_mixin() = default;

    template<typename Props>
    request_title_mixin(Props const& props)
    {
    }

    void
    save_title(JSONRequestOutputArchive& archive) const
    {
    }

    void
    load_title(JSONRequestInputArchive& archive)
    {
    }
};

template<>
class request_title_mixin<true>
{
 public:
    std::string
    get_introspection_title() const
    {
        return title_;
    }

 protected:
    request_title_mixin() = default;

    template<typename Props>
    request_title_mixin(Props const& props) : title_{props.get_title()}
    {
    }

    void
    save_title(JSONRequestOutputArchive& archive) const
    {
        archive(cereal::make_nvp("title", title_));
    }

    void
    load_title(JSONRequestInputArchive& archive)
    {
        archive(cereal::make_nvp("title", title_));
    }

 private:
    std::string title_;
};

} // namespace detail

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
    : public detail::request_title_mixin<Props::introspective>
{
 public:
    static_assert(!std::is_reference_v<Value>);
    using mixin_type = detail::request_title_mixin<Props::introspective>;
    using element_type = function_request_erased;
    using value_type = Value;
    using intf_type = function_request_intf<Value>;
    using props_type = Props;

    static constexpr caching_level_type caching_level = Props::level;
    static constexpr bool introspective = Props::introspective;
    static constexpr bool is_proxy = Props::for_proxy;

    // It is not possible to pass a C-style string as argument.
    template<typename CtorProps, typename Function, typename... Args>
        requires(!is_proxy)
    function_request_erased(
        CtorProps&& props, Function&& function, Args&&... args)
        : mixin_type{props}
    {
        static_assert(std::is_same_v<std::remove_cvref_t<CtorProps>, Props>);
        using impl_type = function_request_impl<
            Value,
            Props::for_coroutine,
            std::remove_cvref_t<Function>,
            std::remove_cvref_t<Args>...>;
        impl_ = std::make_shared<impl_type>(
            std::forward<CtorProps>(props).get_uuid(),
            std::forward<Function>(function),
            std::forward<Args>(args)...);
    }

    template<typename CtorProps, typename... Args>
        requires(is_proxy && caching_level == caching_level_type::none)
    function_request_erased(CtorProps&& props, Args&&... args)
        : mixin_type{props}
    {
        using impl_type
            = proxy_request_uncached<Value, std::remove_cvref_t<Args>...>;
        impl_ = std::make_shared<impl_type>(
            std::forward<CtorProps>(props).get_uuid(),
            std::forward<Args>(args)...);
    }

    template<typename CtorProps, typename... Args>
        requires(is_proxy && caching_level != caching_level_type::none)
    function_request_erased(CtorProps&& props, Args&&... args)
        : mixin_type{props}
    {
        using impl_type
            = proxy_request_cached<Value, std::remove_cvref_t<Args>...>;
        impl_ = std::make_shared<impl_type>(
            std::forward<CtorProps>(props).get_uuid(),
            std::forward<Args>(args)...);
    }

    void
    register_uuid(
        seri_registry& registry,
        catalog_id cat_id,
        std::shared_ptr<seri_resolver_intf> resolver) const
    {
        impl_->register_uuid(registry, cat_id, std::move(resolver));
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

    // TODO try to apply the requires to every function only needed for caching
    captured_id
    get_captured_id() const
        requires(caching_level != caching_level_type::none)
    {
        return captured_id{impl_};
    }

    request_uuid const&
    get_uuid() const
    {
        return impl_->get_uuid();
    }

    void
    accept(req_visitor_intf& visitor) const
    {
        impl_->accept(visitor);
    }

    cppcoro::task<Value>
    resolve_sync(local_context_intf& ctx) const
    {
        return impl_->resolve_sync(ctx);
    }

    cppcoro::task<Value>
    resolve_async(local_async_context_intf& ctx) const
    {
        return impl_->resolve_async(ctx);
    }

 public:
    // Interface for cereal

    // Used for creating placeholder subrequests in the catalog;
    // also called when deserializing a subrequest.
    function_request_erased() = default;

    // Construct object, deserializing from a cereal archive
    // Convenience constructor for when this is the "outer" object.
    // Equivalent alternative:
    //   function_request_erased<...> req;
    //   req.load(archive)
    explicit function_request_erased(JSONRequestInputArchive& archive)
    {
        load(archive);
    }

    void
    save(JSONRequestOutputArchive& archive) const
    {
        // At least for JSON, there is no difference between multiple archive()
        // calls, or putting everything in one call.
        impl_->get_uuid().save_with_name(archive, "uuid");
        this->save_title(archive);
        impl_->save(archive);
    }

    // We always create JSONRequestInputArchive objects for deserializing
    // requests, but cereal only knows about cereal::JSONInputArchive.
    void
    load(cereal::JSONInputArchive& counted_archive)
    {
        auto& archive{static_cast<JSONRequestInputArchive&>(counted_archive)};
        auto& resources{archive.get_resources()};
        auto the_seri_registry{resources.get_seri_registry()};
        auto uuid{request_uuid::load_with_name(archive, "uuid")};
        this->load_title(archive);
        // Create a mostly empty function_request_impl object. uuid defines its
        // exact type (function_request_impl class instantiation).
        impl_ = the_seri_registry->create<intf_type>(std::move(uuid));
        // Deserialize the remainder of the function_request_impl object.
        impl_->load(archive);
    }

 private:
    std::shared_ptr<intf_type> impl_;
};

template<typename Value, typename Props>
struct arg_type_struct<function_request_erased<Value, Props>>
{
    using value_type = Value;
};

// arg should result from normalize_arg()
template<typename Value, typename Props>
void
register_uuid_for_normalized_arg(
    seri_registry& registry,
    catalog_id cat_id,
    function_request_erased<Value, Props> const& arg)
{
    using arg_t = function_request_erased<Value, Props>;
    arg.register_uuid(
        registry, cat_id, std::make_shared<seri_resolver_impl<arg_t>>());
}

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
    requires std::remove_cvref_t<Props>::for_plain_function
auto
rq_function_erased(Props&& props, Function&& function, Args&&... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return function_request_erased<Value, std::remove_cvref_t<Props>>{
        std::forward<Props>(props),
        std::forward<Function>(function),
        std::forward<Args>(args)...};
}

// Creates a type-erased request for a function that is a coroutine.
template<typename Props, typename Function, typename... Args>
    requires std::remove_cvref_t<Props>::for_coroutine
auto
rq_function_erased(Props&& props, Function&& function, Args&&... args)
{
    // Rely on the coroutine returning cppcoro::task<Value>,
    // which is a class that has a value_type member.
    // Function's first argument is a reference to some context type.
    using Value = typename std::invoke_result_t<
        Function,
        context_intf&,
        arg_type<Args>...>::value_type;
    return function_request_erased<Value, std::remove_cvref_t<Props>>{
        std::forward<Props>(props),
        std::forward<Function>(function),
        std::forward<Args>(args)...};
}

// Creates a type-erased proxy request.
template<typename Value, typename Props, typename... Args>
    requires std::remove_cvref_t<Props>::for_proxy
auto
rq_function_erased(Props&& props, Args&&... args)
{
    return function_request_erased<Value, std::remove_cvref_t<Props>>{
        std::forward<Props>(props), std::forward<Args>(args)...};
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
 * of the main function_request_impl template class. Each variant needs its
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
 *
 * function_request_impl's resolve_sync() and resolve_async() optimize
 * these requests so that the resolution overhead should be minimal, and
 * there should be no need for identity_func() or identity_coro() calls.
 */

// Checks that Arg is a value of type ValueType, or a request resolving to that
// type.
template<typename Arg, typename ValueType>
concept TypedArg
    = std::convertible_to<Arg, ValueType>
      || (std::same_as<typename Arg::value_type, ValueType>
          && std::same_as<
              function_request_erased<ValueType, typename Arg::props_type>,
              Arg>)
      || (std::same_as<typename Arg::value_type, ValueType>
          && std::same_as<value_request<ValueType>, Arg>);

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

namespace detail {

// Creates a uuid for a normalization request. If this is a proxy request, the
// uuid also applies to the real request.
template<typename Value, typename Props>
auto
make_normalization_uuid()
{
    constexpr bool for_coro{
        Props::function_type == request_function_t::coro
        || Props::function_type == request_function_t::proxy_coro};
    using V = std::remove_cvref_t<Value>;
    return request_uuid{
        for_coro ? normalization_uuid_str<V>::coro
                 : normalization_uuid_str<V>::func};
}

// Creates a request_props object for a non-introspective normalization
// request.
template<typename Value, typename Props>
auto
make_normalization_props()
    requires(!Props::introspective)
{
    return Props{make_normalization_uuid<Value, Props>()};
}

// Creates a request_props object for an introspective normalization request.
template<typename Value, typename Props>
auto
make_normalization_props()
    requires Props::introspective
{
    return Props{make_normalization_uuid<Value, Props>(), "arg"};
}

// Creates a normalization request in a non-coroutine context.
template<typename Props, typename Value>
auto
make_normalization_request(Value&& arg)
    requires Props::for_plain_function
{
    using V = std::remove_cvref_t<Value>;
    // TODO instantiating identity_func looks suspicious
    return rq_function_erased(
        make_normalization_props<V, Props>(),
        identity_func<V>,
        std::forward<Value>(arg));
}

// Creates a normalization request in a coroutine context.
template<typename Props, typename Value>
auto
make_normalization_request(Value&& arg)
    requires Props::for_coroutine
{
    using V = std::remove_cvref_t<Value>;
    return rq_function_erased(
        make_normalization_props<V, Props>(), identity_coro<V>, arg);
}

// Creates a normalization request in a proxy context.
template<typename Props, typename Value>
auto
make_normalization_request(Value&& arg)
    requires Props::for_proxy
{
    using V = std::remove_cvref_t<Value>;
    return rq_function_erased<V>(make_normalization_props<V, Props>(), arg);
}

} // namespace detail

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
 * Props is a request_props instantiation, and must be the one for the main
 * request.
 *
 * The uuid for the main request defines the Props for the main request's
 * function_request_erased class, and for all arguments that are subrequests.
 * The uuid for a subrequest need only define the function_request_impl
 * instantiation. In normalization requests, the function is fixed
 * (identity_func or identity_coro), meaning the uuid depends only on:
 * - The Value type
 * - Whether the function is a "normal" one or a coroutine
 */

// Normalizes a value argument.
// If Value is std::string, arg may be a C-style string, which is converted to
// and stored as std::string.
template<typename Value, typename Props>
    requires(!Request<Value>)
auto normalize_arg(Value&& arg)
{
    return detail::make_normalization_request<Props>(std::forward<Value>(arg));
}

// Normalizes a value_request argument.
template<typename Value, typename Props, typename Arg>
    requires std::is_same_v<std::remove_cvref_t<Arg>, value_request<Value>>
auto
normalize_arg(Arg&& arg)
{
    return detail::make_normalization_request<Props>(
        std::forward<Arg>(arg).get_value());
}

// Normalizes a function_request_erased argument (returned as-is).
// If a subrequest is passed as argument, its Props must equal those for the
// main request.
template<typename Value, typename Props, typename Arg>
    requires std::is_same_v<
        std::remove_cvref_t<Arg>,
        function_request_erased<Value, Props>>
auto
normalize_arg(Arg&& arg)
{
    return std::forward<Arg>(arg);
}

} // namespace cradle

#endif
