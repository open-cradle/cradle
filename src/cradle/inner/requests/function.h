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

class conflicting_types_uuid_error : public uuid_error
{
 public:
    using uuid_error::uuid_error;
};

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
template<typename Value>
class function_request_intf : public id_interface
{
 public:
    virtual ~function_request_intf() = default;

    virtual request_uuid
    get_uuid() const
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
 * A uuid identifies three functions:
 * - create() creates a shared_ptr<function_request_impl> object.
 * - save() serializes a function_request_impl object to JSON.
 * - load() deserializes a function_request_impl object from JSON.
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
 */
class cereal_functions_registry_impl
{
 public:
    using create_t = std::shared_ptr<void>(request_uuid const& uuid);
    using save_t = void(cereal::JSONOutputArchive& archive, void const* impl);
    using load_t = void(cereal::JSONInputArchive& archive, void* impl);

    struct entry_t
    {
        create_t* create;
        save_t* save;
        load_t* load;
    };

    static cereal_functions_registry_impl&
    instance();

    void
    add_entry(
        std::string const& uuid_str,
        create_t* create,
        save_t* save,
        load_t* load);

    entry_t&
    find_entry(request_uuid const& uuid);

 private:
    std::unordered_map<std::string, entry_t> entries_;
};

template<typename Intf>
class cereal_functions_registry
{
    using impl_t = cereal_functions_registry_impl;
    using entry_t = typename impl_t::entry_t;
    using create_t = typename impl_t::create_t;
    using save_t = typename impl_t::save_t;
    using load_t = typename impl_t::load_t;

 public:
    static cereal_functions_registry&
    instance()
    {
        static cereal_functions_registry instance_;
        return instance_;
    }

    cereal_functions_registry() : impl_{impl_t::instance()}
    {
    }

    void
    add_entry(
        std::string const& uuid_str,
        create_t* create,
        save_t* save,
        load_t* load)
    {
        impl_.add_entry(uuid_str, create, save, load);
    }

    std::shared_ptr<Intf>
    create(request_uuid const& uuid)
    {
        entry_t& entry{impl_.find_entry(uuid)};
        return std::static_pointer_cast<Intf>(entry.create(uuid));
    }

    void
    save(cereal::JSONOutputArchive& archive, Intf const& intf)
    {
        entry_t& entry{impl_.find_entry(intf.get_uuid())};
        entry.save(archive, &intf);
    }

    // The uuid should be set before deserializing the (rest of) the object.
    void
    load(cereal::JSONInputArchive& archive, Intf& intf)
    {
        entry_t& entry{impl_.find_entry(intf.get_uuid())};
        entry.load(archive, &intf);
    }

 private:
    impl_t& impl_;
};

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
 */
template<typename Value, bool AsCoro, typename Function, typename... Args>
class function_request_impl : public function_request_intf<Value>
{
 public:
    using this_type = function_request_impl;
    using base_type = function_request_intf<Value>;

    static constexpr bool func_is_coro = AsCoro;
    static constexpr bool func_is_plain
        = std::is_function_v<std::remove_pointer_t<Function>>;

    function_request_impl(request_uuid uuid, Function function, Args... args)
        : uuid_{std::move(uuid)}, args_{std::move(args)...}
    {
        // The uuid uniquely identifies the function.
        // Store it so that is available during deserialization.
        auto uuid_str{uuid_.str()};
        std::scoped_lock lock{static_mutex_};
        cereal_functions_registry<base_type>::instance().add_entry(
            uuid_str, create, save_json, load_json);

        auto it = matching_functions_.find(uuid_str);
        if (it == matching_functions_.end())
        {
            function_ = std::make_shared<Function>(std::move(function));
            matching_functions_[uuid_str] = function_;
        }
        else
        {
            function_ = matching_functions_[uuid_str];
            // Try to catch attempts to associate different functions with the
            // same uuid.
            // - This is possible for plain C++ functions.
            // - It is not possible to distinguish two lambdas capturing
            //   different arguments (and thus probably having different
            //   behaviour).
            // - Captureless lambdas with the same type are identical.
            if constexpr (func_is_plain)
            {
                if (*function_ != function)
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
        using Indices = std::index_sequence_for<Args...>;
        visit_args(visitor, args_, Indices{});
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
            // gcc tends to have trouble with this piece of code (leading to
            // compiler crashes or runtime errors).
            ResolutionConstraintsLocalSync constraints;
            co_return co_await std::apply(
                [&](auto&&... args) -> cppcoro::task<Value> {
                    co_return (*function_)((co_await resolve_request(
                        ctx,
                        std::forward<decltype(args)>(args),
                        constraints))...);
                },
                args_);
        }
        else
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
    }

    cppcoro::task<Value>
    resolve_async(local_async_context_intf& ctx) const override
    {
        if constexpr (!func_is_coro)
        {
            // TODO resolve_async for non-coro function
            ResolutionConstraintsLocalSync constraints;
            co_return co_await std::apply(
                [&](auto&&... args) -> cppcoro::task<Value> {
                    co_return (*function_)((co_await resolve_request(
                        ctx,
                        std::forward<decltype(args)>(args),
                        constraints))...);
                },
                args_);
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
    }

 public:
    // cereal-related

    // Construct an object to be deserialized.
    // The uuid is serialized in function_request_erased, not here.
    function_request_impl(request_uuid const& uuid) : uuid_{uuid}
    {
    }

    static std::shared_ptr<void>
    create(request_uuid const& uuid)
    {
        return std::make_shared<this_type>(uuid);
    }

    static void
    save_json(cereal::JSONOutputArchive& archive, void const* impl)
    {
        static_cast<this_type const*>(impl)->save(archive);
    }

    static void
    load_json(cereal::JSONInputArchive& archive, void* impl)
    {
        static_cast<this_type*>(impl)->load(archive);
    }

    template<typename Archive>
    void
    save(Archive& archive) const
    {
        archive(cereal::make_nvp("args", args_));
    }

    template<typename Archive>
    void
    load(Archive& archive)
    {
        archive(cereal::make_nvp("args", args_));
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
    // Protector of matching_functions_
    // TODO check this mutex; it should be global, not one per instantiation
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
    bool Introspective = false>
struct request_props
{
    static constexpr caching_level_type level = Level;
    static constexpr bool func_is_coro = AsCoro;
    static constexpr bool introspective = Introspective;

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

// Serialize a function_request_impl object, given a type-erased
// function_request_intf reference.
template<typename Archive, typename Value>
void
save(Archive& archive, function_request_intf<Value> const& intf)
{
    using intf_type = function_request_intf<Value>;
    cereal_functions_registry<intf_type>::instance().save(archive, intf);
}

// Deserialize a function_request_impl object, given a type-erased
// function_request_intf reference.
template<typename Archive, typename Value>
void
load(Archive& archive, function_request_intf<Value>& intf)
{
    using intf_type = function_request_intf<Value>;
    cereal_functions_registry<intf_type>::instance().load(archive, intf);
}

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

    template<typename Function, typename... Args>
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
    template<typename Archive>
    explicit function_request_erased(Archive& archive)
    {
        load(archive);
    }

    template<typename Archive>
    void
    save(Archive& archive) const
    {
        // At least for JSON, there is no difference between multiple archive()
        // calls, or putting everything in one call.
        impl_->get_uuid().save_with_name(archive, "uuid");
        archive(cereal::make_nvp("title", title_));
        ::cradle::save(archive, *impl_);
    }

    template<typename Archive>
    void
    load(Archive& archive)
    {
        auto uuid{request_uuid::load_with_name(archive, "uuid")};
        archive(cereal::make_nvp("title", title_));
        // Create a mostly empty function_request_impl object. uuid defines its
        // exact type (function_request_impl class instantiation), which could
        // be different from the one with which this function_request_erased
        // instantiation was registered.
        impl_ = cereal_functions_registry<intf_type>::instance().create(
            std::move(uuid));
        // Deserialize the remainder of the function_request_impl object.
        ::cradle::load(archive, *impl_);
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
    requires(Props::func_is_coro)
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
    requires(!Request<Value> && !Props::func_is_coro)
auto normalize_arg(Value const& arg)
{
    Props props{make_normalization_uuid<Value, false>(), "arg"};
    return rq_function_erased(std::move(props), identity_func<Value>, arg);
}

// Normalizes a value argument in a coroutine context.
template<typename Value, typename Props>
    requires(!Request<Value> && Props::func_is_coro)
auto normalize_arg(Value const& arg)
{
    Props props{make_normalization_uuid<Value, true>(), "arg"};
    return rq_function_erased(std::move(props), identity_coro<Value>, arg);
}

// Normalizes a C-style string argument in a non-coroutine context.
template<typename Value, typename Props>
    requires(std::same_as<Value, std::string> && !Props::func_is_coro)
auto normalize_arg(char const* arg)
{
    Props props{make_normalization_uuid<Value, false>(), "arg"};
    return rq_function_erased(
        std::move(props), identity_func<std::string>, std::string{arg});
}

// Normalizes a C-style string argument in a coroutine context.
template<typename Value, typename Props>
    requires(std::same_as<Value, std::string> && Props::func_is_coro)
auto normalize_arg(char const* arg)
{
    Props props{make_normalization_uuid<Value, true>(), "arg"};
    return rq_function_erased(
        std::move(props), identity_coro<std::string>, std::string{arg});
}

// Normalizes a value_request argument in a non-coroutine context.
template<typename Value, typename Props>
    requires(!Props::func_is_coro)
auto normalize_arg(value_request<Value> const& arg)
{
    Props props{make_normalization_uuid<Value, false>(), "arg"};
    return rq_function_erased(
        std::move(props), identity_func<Value>, arg.get_value());
}

// Normalizes a value_request argument in a coroutine context.
template<typename Value, typename Props>
    requires(Props::func_is_coro)
auto normalize_arg(value_request<Value> const& arg)
{
    Props props{make_normalization_uuid<Value, true>(), "arg"};
    return rq_function_erased(
        std::move(props), identity_coro<Value>, arg.get_value());
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
