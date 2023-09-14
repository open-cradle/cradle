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
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/resolve/resolve_request.h>

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

class unregistered_uuid_error : public uuid_error
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
 * The references taken by resolve_sync() and resolve_async() are the "minimal"
 * ones needed.
 */
typedef void
unregister_function_t(std::string const& uuid_str);

template<typename Value>
class function_request_intf : public id_interface
{
 public:
    virtual ~function_request_intf() = default;

    virtual request_uuid
    get_uuid() const
        = 0;

    virtual void
    register_uuid(std::string const& cat_name)
        = 0;

    virtual void
    save(cereal::JSONOutputArchive& archive) const
        = 0;

    virtual void
    load(cereal::JSONInputArchive& archive)
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

/**
 * A registry of functions to serialize or deserialize a function_request_impl
 * object. The functions are identified by a request_uuid value.
 *
 * A uuid identifies two functions:
 * - create() creates a shared_ptr<function_request_impl> object.
 * - unregisters() removes data associated with the uuid, destroying the
 * ability to resolve the uuid.
 *
 * This registry forms the basis for an ad-hoc alternative to the cereal
 * polymorphic types implementation, which is not usable here:
 * 1. A polymorphic type needs to be registered with CEREAL_REGISTER_TYPE or
 *    CEREAL_REGISTER_TYPE_WITH_NAME. These put constructs in namespaces, so
 *    cannot be used within a class or function.
 *    However, with templated classes like function_request_impl, we cannot use
 *    these constructs outside a class either, as the type of an instance is
 *    known only _within_ the class.
 * 2. Cereal needs to translate the type of a derived class to the name under
 *    which that polymorphic type will be serialized.
 *    - We cannot use type_index::name() because that is not portable, and
 *      because creating a serialized request (e.g. from a WebSocket client)
 *      would be practically impossible.
 *    - We cannot use the uuid because that is unique for a
 *      function_request_impl+function combination, not for just
 *      function_request_impl.
 * Instead, we need a mechanism where the uuid in the serialization identifies
 * both the function_request_impl _class_ (template instantiation), and the
 * function _value_ in that class.
 *
 * All functions in this class's API are thread-safe.
 */
class cereal_functions_registry
{
 public:
    using create_t = std::shared_ptr<void>(request_uuid const& uuid);
    using unregister_t = void(std::string const& uuid_str);

    struct entry_t
    {
        std::string cat_name;
        create_t* create;
        unregister_t* unregister;
    };

    static cereal_functions_registry&
    instance();

    void
    add_entry(
        std::string const& cat_name,
        std::string const& uuid_str,
        create_t* create,
        unregister_t* unregister);

    void
    unregister_catalog(std::string const& cat_name);

    // Intf should be a function_request_intf instantiation.
    template<typename Intf>
    std::shared_ptr<Intf>
    create(request_uuid const& uuid)
    {
        entry_t& entry{find_entry(uuid)};
        return std::static_pointer_cast<Intf>(entry.create(uuid));
    }

 private:
    using entries_t = std::unordered_map<std::string, entry_t>;
    entries_t entries_;
    std::mutex mutex_;

    entry_t&
    find_entry(request_uuid const& uuid);
};

template<typename Head, typename... Tail>
struct first_element
{
    using type = Head;
};

// This function should be a no-op unless the argument results from a
// normalize_arg() call.
template<typename Arg>
void
register_uuid_for_normalized_arg(std::string const& cat_name, Arg const& arg)
{
}

template<typename Args, std::size_t... Ix>
void
register_uuid_for_normalized_args(
    std::string const& cat_name, Args const& args, std::index_sequence<Ix...>)
{
    (register_uuid_for_normalized_arg(cat_name, std::get<Ix>(args)), ...);
}

/*
 * The actual type created by function_request_erased, but visible only in its
 * constructor (and erased elsewhere).
 *
 * Intf is a function_request_intf instantiation.
 * Function must be MoveAssignable but may not be CopyAssignable (e.g. if it's
 * a lambda).
 *
 * No part of this class depends on the actual context type used to resolve the
 * request.
 *
 * A function_request_impl object has an associated uuid that should identify
 * both the function_request_impl instantiation (type), and function value.
 * It is primarily the caller's responsibility to provide a unique uuid.
 * The implementation will catch attempts to associate different instantiations
 * with a single uuid, but in general cannot catch attempts to associate
 * different function values with a single uuid. This will still happen in case
 * the function is a plain C++ function, but is impossible if it is a capturing
 * lambda.
 *
 * The uuid identifying both function_request_impl class and function value
 * leads to a two-step deserialization process: first a function_request_impl
 * object is created of the specified class, then that object's function member
 * is set to the correct value.
 *
 * This class owns two singletons: maching_functions_mutex_ and
 * matching_functions_. In case of DLLs, the dynamic loader will use a single,
 * global, matching_functions_ variable for all identical function_request_impl
 * instantiations (Linux), or one variable per DLL (Windows). In the Linux
 * case, when a DLL is unloaded, the global variable's function pointers would
 * become dangling. In the Windows case, the set of all known uuid's is spread
 * over several variables, but each variable will be consulted only for the
 * uuid's supported by the corresponding DLL.
 */
template<typename Value, bool AsCoro, typename Function, typename... Args>
class function_request_impl : public function_request_intf<Value>
{
 public:
    using this_type = function_request_impl;
    using base_type = function_request_intf<Value>;
    using ArgIndices = std::index_sequence_for<Args...>;

    static constexpr bool func_is_coro = AsCoro;
    static constexpr bool func_is_plain
        = std::is_function_v<std::remove_pointer_t<Function>>;

    // function has to be stored in this object so that the request can be
    // resolved. If this object is created in a "register resolver" context,
    // function must also be stored in matching_functions_. function should be
    // moveable but not necessarily copyable; the solution is to put it in a
    // shared_ptr wrapper.
    function_request_impl(request_uuid uuid, Function function, Args... args)
        : uuid_{std::move(uuid)},
          function_{std::make_shared<Function>(std::move(function))},
          args_{std::move(args)...}
    {
    }

    void
    register_uuid(std::string const& cat_name) override
    {
        auto uuid_str{uuid_.str()};
        // TODO add_entry() must throw if an entry already exists for uuid_str.
        // An alternative would be to associate a DLL id with the entry, making
        // it possible to have resolvers for one uuid in multiple DLLs.
        cereal_functions_registry::instance().add_entry(
            cat_name, uuid_str, create, unregister);

        std::scoped_lock lock{maching_functions_mutex_};
        if (matching_functions_.find(uuid_str) != matching_functions_.end())
        {
            // Should not happen; maybe an earlier exception prevented the
            // unregister() call.
            fmt::print("ERR: multiple C++ functions for uuid {}\n", uuid_str);
        }
        // matching_functions_ should not yet have an entry for uuid_str; if it
        // does, the existing entry cannot be trusted, so replace it.
        matching_functions_.insert_or_assign(uuid_str, function_);

        // Register uuids for any args resulting from normalize_arg()
        // TODO how can we unregister these? Do we need to?
        register_uuid_for_normalized_args(cat_name, args_, ArgIndices{});
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

    request_uuid
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
    function_request_impl(request_uuid const& uuid) : uuid_{uuid}
    {
    }

    static std::shared_ptr<void>
    create(request_uuid const& uuid)
    {
        return std::make_shared<this_type>(uuid);
    }

    void
    save(cereal::JSONOutputArchive& archive) const override
    {
        archive(cereal::make_nvp("args", args_));
    }

    void
    load(cereal::JSONInputArchive& archive) override
    {
        archive(cereal::make_nvp("args", args_));
        std::scoped_lock lock{maching_functions_mutex_};
        auto it = matching_functions_.find(uuid_.str());
        if (it == matching_functions_.end())
        {
            // This cannot happen.
            throw no_function_for_uuid_error(
                fmt::format("No function found for uuid {}", uuid_.str()));
        }
        function_ = it->second;
    }

 public:
    // DLL-related

    // Deallocate all resources associated with uuid_str.
    // Remove all references and pointers related to uuid, in the singletons
    // associated with this class instantiation. This must be called when the
    // DLL able to resolve uuid is unloaded, to prevent dangling pointers in
    // matching_functions_.
    //
    // For Windows(-like) dynamic loaders, these singletons are destroyed when
    // the DLL is unloaded, so calling this function would be redundant.
    //
    // For Linux(-like) dynamic loaders, these pointers become dangling on DLL
    // unload, but as the framework cannot resolve the corresponding uuid
    // anymore after, they should be unreachable. When a DLL is (re-)loaded
    // that can resolve the uuid, the pointers get overwritten. So there should
    // be no real need to call this function, but removing the dangling
    // pointers is cleaner and safer.
    static void
    unregister(std::string const& uuid_str)
    {
        std::scoped_lock lock{maching_functions_mutex_};
        matching_functions_.erase(uuid_str);
    }

 private:
    // Protector of matching_functions_
    static inline std::mutex maching_functions_mutex_;

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
 * Deserializing a proxy request doesn't make sense. They must not (cannot,
 * once the TODO in class seri_catalog is solved) be registered as seri
 * resolvers, which means the create and load will never be accessed.
 *
 * TODO consider inheriting function_request_impl from proxy_request_base
 */
template<typename Value, typename... Args>
class proxy_request_base : public function_request_intf<Value>
{
 public:
    using this_type = proxy_request_base;
    using intf_type = function_request_intf<Value>;

    // uuid must be valid (these objects are meant to be serialized)
    proxy_request_base(request_uuid uuid, Args... args)
        : uuid_{std::move(uuid)}, args_{std::move(args)...}
    {
    }

    // Proxy requests should not be registered.
    void
    register_uuid(std::string const& cat_name) override
    {
        throw not_implemented_error{"proxy_request_base::register_uuid"};
    }

    request_uuid
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
    save(cereal::JSONOutputArchive& archive) const override
    {
        archive(cereal::make_nvp("args", args_));
    }

    void
    load(cereal::JSONInputArchive& archive) override
    {
        throw not_implemented_error{"proxy_request_base::load"};
    }

 protected:
    request_uuid uuid_;
    std::tuple<Args...> args_;
};

template<typename Value, typename... Args>
class proxy_request_uncached : public proxy_request_base<Value, Args...>
{
 public:
    using base_type = proxy_request_base<Value, Args...>;
    proxy_request_uncached(request_uuid uuid, Args... args)
        : base_type{std::move(uuid), std::move(args)...}
    {
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
    proxy_request_cached(request_uuid uuid, Args... args)
        : base_type{std::move(uuid), std::move(args)...}
    {
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

void
check_title_is_valid(std::string const& title);

// Request (resolution) properties that would be identical between similar
// requests
// - Request attributes (AsCoro)
// - How it should be resolved (Level, Introspective)
// All requests in a tree (main request, subrequests) must have the same
// request_props, so that the main request's uuid defines the complete
// request type.
template<
    caching_level_type Level,
    bool AsCoro = false,
    bool Introspective = false,
    bool IsProxy = false>
struct request_props
{
    static constexpr caching_level_type level = Level;
    static constexpr bool func_is_coro = AsCoro;
    static constexpr bool introspective = Introspective;
    static constexpr bool is_proxy = IsProxy;

    request_uuid uuid_;
    std::string title_; // Used only if introspective

#if 0
    // Constructor for a request that won't be serialized, won't be fully
    // cached, and won't be introspected
    // Currently no callers. The request_uuid default ctor is private.
    // If needed after all, putting request_uuid in a unique_ptr might
    // be better than creating a request_uuid with an empty string.
    request_props()
    {
        static_assert(level != caching_level_type::full);
        static_assert(!introspective);
    }
#endif

    // Constructor for a request that
    // - Can be serialized
    // - Can be fully cached (but will be only if the Level template argument
    //   is caching_level_type::full)
    // - Cannnot be introspected
    explicit request_props(request_uuid uuid) : uuid_{std::move(uuid)}
    {
        static_assert(!introspective);
    }

    // Constructor for a request that
    // - Can be serialized
    // - Can be fully cached (but will be only if the Level template argument
    //   is caching_level_type::full)
    // - Can be introspected (but will be only if the Introspective template
    //   argument holds true)
    request_props(request_uuid uuid, std::string title)
        : uuid_{std::move(uuid)}, title_{std::move(title)}
    {
        check_title_is_valid(title_);
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
    using intf_type = function_request_intf<Value>;
    using props_type = Props;

    static constexpr caching_level_type caching_level = Props::level;
    static constexpr bool introspective = Props::introspective;
    static constexpr bool is_proxy = Props::is_proxy;

    template<typename Function, typename... Args>
        requires(!is_proxy)
    function_request_erased(Props props, Function function, Args... args)
        : title_{std::move(props.title_)}
    {
        using impl_type = function_request_impl<
            Value,
            Props::func_is_coro,
            Function,
            Args...>;
        impl_ = std::make_shared<impl_type>(
            std::move(props.uuid_), std::move(function), std::move(args)...);
        init_captured_id();
    }

    template<typename... Args>
        requires(is_proxy && caching_level == caching_level_type::none)
    function_request_erased(Props props, Args... args)
        : title_{std::move(props.title_)}
    {
        using impl_type = proxy_request_uncached<Value, Args...>;
        impl_ = std::make_shared<impl_type>(
            std::move(props.uuid_), std::move(args)...);
    }

    template<typename... Args>
        requires(is_proxy && caching_level != caching_level_type::none)
    function_request_erased(Props props, Args... args)
        : title_{std::move(props.title_)}
    {
        using impl_type = proxy_request_cached<Value, Args...>;
        impl_ = std::make_shared<impl_type>(
            std::move(props.uuid_), std::move(args)...);
        init_captured_id();
    }

    void
    register_uuid(std::string const& cat_name) const
    {
        impl_->register_uuid(cat_name);
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
    explicit function_request_erased(cereal::JSONInputArchive& archive)
    {
        load(archive);
    }

    void
    save(cereal::JSONOutputArchive& archive) const
    {
        // At least for JSON, there is no difference between multiple archive()
        // calls, or putting everything in one call.
        impl_->get_uuid().save_with_name(archive, "uuid");
        archive(cereal::make_nvp("title", title_));
        impl_->save(archive);
    }

    // This gets called directly from cereal, for subrequests.
    // No additional arguments are possible, so the uuid-to-creator map must be
    // in some global registry that can be accessed here.
    void
    load(cereal::JSONInputArchive& archive)
    {
        auto uuid{request_uuid::load_with_name(archive, "uuid")};
        archive(cereal::make_nvp("title", title_));
        // Create a mostly empty function_request_impl object. uuid defines its
        // exact type (function_request_impl class instantiation), which could
        // be different from the one with which this function_request_erased
        // instantiation was registered.
        impl_ = cereal_functions_registry::instance().create<intf_type>(
            std::move(uuid));
        // Deserialize the remainder of the function_request_impl object.
        impl_->load(archive);
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

// Arg should result from normalize_arg()
template<typename Value, typename Props>
void
register_uuid_for_normalized_arg(
    std::string const& cat_name,
    function_request_erased<Value, Props> const& arg)
{
    arg.register_uuid(cat_name);
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
    requires(!Props::func_is_coro && !Props::is_proxy)
auto rq_function_erased(Props props, Function function, Args... args)
{
    using Value = std::invoke_result_t<Function, arg_type<Args>...>;
    return function_request_erased<Value, Props>{
        std::move(props), std::move(function), std::move(args)...};
}

// Creates a type-erased request for a function that is a coroutine.
template<typename Props, typename Function, typename... Args>
    requires(Props::func_is_coro && !Props::is_proxy)
auto rq_function_erased(Props props, Function function, Args... args)
{
    // Rely on the coroutine returning cppcoro::task<Value>,
    // which is a class that has a value_type member.
    // Function's first argument is a reference to some context type.
    using Value = typename std::invoke_result_t<
        Function,
        context_intf&,
        arg_type<Args>...>::value_type;
    return function_request_erased<Value, Props>{
        std::move(props), std::move(function), std::move(args)...};
}

// Creates a type-erased proxy request.
template<typename Value, typename Props, typename... Args>
    requires(Props::is_proxy)
auto rq_function_erased(Props props, Args... args)
{
    return function_request_erased<Value, Props>{
        std::move(props), std::move(args)...};
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

template<typename Value, bool func_is_coro>
auto
make_normalization_uuid()
{
    return request_uuid{
        func_is_coro ? normalization_uuid_str<Value>::coro
                     : normalization_uuid_str<Value>::func};
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

// Normalizes a value argument in a non-coroutine context.
template<typename Value, typename Props>
    requires(!Request<Value> && !Props::func_is_coro && !Props::is_proxy)
auto normalize_arg(Value const& arg)
{
    Props props{make_normalization_uuid<Value, false>(), "arg"};
    return rq_function_erased(std::move(props), identity_func<Value>, arg);
}

// Normalizes a value argument in a coroutine context.
template<typename Value, typename Props>
    requires(!Request<Value> && Props::func_is_coro && !Props::is_proxy)
auto normalize_arg(Value const& arg)
{
    Props props{make_normalization_uuid<Value, true>(), "arg"};
    return rq_function_erased(std::move(props), identity_coro<Value>, arg);
}

// Normalizes a value argument in a proxy context.
template<typename Value, typename Props>
    requires(!Request<Value> && Props::is_proxy)
auto normalize_arg(Value const& arg)
{
    // TODO what about Props::func_is_coro ?
    Props props{make_normalization_uuid<Value, Props::func_is_coro>(), "arg"};
    return rq_function_erased<Value>(std::move(props), arg);
}

// Normalizes a C-style string argument in a non-coroutine context.
template<typename Value, typename Props>
    requires(
        std::same_as<Value, std::string> && !Props::func_is_coro
        && !Props::is_proxy)
auto normalize_arg(char const* arg)
{
    Props props{make_normalization_uuid<Value, false>(), "arg"};
    return rq_function_erased(
        std::move(props), identity_func<std::string>, std::string{arg});
}

// Normalizes a C-style string argument in a coroutine context.
template<typename Value, typename Props>
    requires(
        std::same_as<Value, std::string> && Props::func_is_coro
        && !Props::is_proxy)
auto normalize_arg(char const* arg)
{
    Props props{make_normalization_uuid<Value, true>(), "arg"};
    return rq_function_erased(
        std::move(props), identity_coro<std::string>, std::string{arg});
}

// Normalizes a C-style string argument in a proxy context.
template<typename Value, typename Props>
    requires(std::same_as<Value, std::string> && Props::is_proxy)
auto normalize_arg(char const* arg)
{
    Props props{make_normalization_uuid<Value, Props::func_is_coro>(), "arg"};
    return rq_function_erased<Value>(std::move(props), std::string{arg});
}

// Normalizes a value_request argument in a non-coroutine context.
template<typename Value, typename Props>
    requires(!Props::func_is_coro && !Props::is_proxy)
auto normalize_arg(value_request<Value> const& arg)
{
    Props props{make_normalization_uuid<Value, false>(), "arg"};
    return rq_function_erased(
        std::move(props), identity_func<Value>, arg.get_value());
}

// Normalizes a value_request argument in a coroutine context.
template<typename Value, typename Props>
    requires(Props::func_is_coro && !Props::is_proxy)
auto normalize_arg(value_request<Value> const& arg)
{
    Props props{make_normalization_uuid<Value, true>(), "arg"};
    return rq_function_erased(
        std::move(props), identity_coro<Value>, arg.get_value());
}

// Normalizes a value_request argument in a proxy context.
template<typename Value, typename Props>
    requires(Props::is_proxy)
auto normalize_arg(value_request<Value> const& arg)
{
    Props props{make_normalization_uuid<Value, Props::func_is_coro>(), "arg"};
    return rq_function_erased<Value>(std::move(props), arg.get_value());
}

// Normalizes a function_request_erased argument (returned as-is).
// If a subrequest is passed as argument, its Props must equal those for the
// main request.
template<typename Value, typename Props, typename Arg>
    requires std::same_as<function_request_erased<Value, Props>, Arg>
auto
normalize_arg(Arg const& arg)
{
    return arg;
}

} // namespace cradle

#endif
