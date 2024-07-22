#ifndef CRADLE_INNER_REQUESTS_GENERIC_H
#define CRADLE_INNER_REQUESTS_GENERIC_H

#include <chrono>
#include <concepts>
#include <memory>
#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/remote/types.h>
#include <cradle/inner/requests/types.h>
#include <cradle/inner/service/config.h>

namespace cradle {

struct immutable_cache;
class inner_resources;
class request_uuid;
class tasklet_tracker;

// Specifies how request resolution results should be cached.
// The specification consists of two parts:
// - Caching level (no caching, memory only, or memory + disk)
// - Type, which is one of
//   - Composition-based: the cache key is based on the argument specifications
//     (subrequests or values).
//   - Value-based: the cache key is based on the argument values.
// Value-based caching probably should be applied to a leaf in a request tree
// only, as it tends to basically disable caching at lower levels.
enum class caching_level_type
{
    // No caching
    none,
    // Caching in local memory only; composition-based
    memory,
    // Caching in local memory, plus some secondary storage; composition-based
    full,
    // Like memory; value-based
    memory_vb,
    // Like full; value-based
    full_vb,
};

// Disallow relational comparisons between caching_level_type values;
// e.g. "full < full_vb" doesn't make sense.
// Also, the is_...() functions below are preferred over == or !=.
bool
operator<(caching_level_type x, caching_level_type y)
    = delete;
bool
operator<=(caching_level_type x, caching_level_type y)
    = delete;
bool
operator>(caching_level_type x, caching_level_type y)
    = delete;
bool
operator>=(caching_level_type x, caching_level_type y)
    = delete;
std::strong_ordering
operator<=>(caching_level_type x, caching_level_type y)
    = delete;

inline constexpr bool
is_uncached(caching_level_type level)
{
    return level == caching_level_type::none;
}

inline constexpr bool
is_cached(caching_level_type level)
{
    return level != caching_level_type::none;
}

inline constexpr bool
is_memory_cached(caching_level_type level)
{
    return level == caching_level_type::memory
           || level == caching_level_type::memory_vb;
}

inline constexpr bool
is_fully_cached(caching_level_type level)
{
    return level == caching_level_type::full
           || level == caching_level_type::full_vb;
}

inline constexpr bool
is_composition_based(caching_level_type level)
{
    return level == caching_level_type::memory
           || level == caching_level_type::full;
}

inline constexpr bool
is_value_based(caching_level_type level)
{
    return level == caching_level_type::memory_vb
           || level == caching_level_type::full_vb;
}

inline constexpr caching_level_type
to_composition_based(caching_level_type level)
{
    if (level == caching_level_type::memory_vb)
    {
        return caching_level_type::memory;
    }
    else if (level == caching_level_type::full_vb)
    {
        return caching_level_type::full;
    }
    else
    {
        return level;
    }
}

// Basic request information
struct request_essentials
{
    request_essentials(std::string uuid_str_arg)
        : uuid_str{std::move(uuid_str_arg)}
    {
    }

    request_essentials(std::string uuid_str_arg, std::string title_arg)
        : uuid_str{std::move(uuid_str_arg)}, title{std::move(title_arg)}
    {
    }

    std::string const uuid_str;
    std::optional<std::string> const title;
};

/*
 * Visits a request's arguments (which may be subrequests themselves).
 *
 * A visitor may contain state information relating to a specific request
 * object. Thus, a new visitor object has to be created for visiting a
 * subrequest's arguments.
 *
 * A request that only has non-request arguments cannot be distinguished from
 * a request that has no arguments at all. Thus, such a request's accept()
 * implementation could be a no-op.
 * TODO try to optimize accept() for "trivial" requests
 * (e.g. those resulting from normalize_arg)
 */
class req_visitor_intf
{
 public:
    virtual ~req_visitor_intf() = default;

    // Visits an argument that is a value (not a subrequest).
    virtual void
    visit_val_arg(std::size_t ix)
        = 0;

    // Visits an argument that is a subrequest.
    // Returns the visitor for the subrequest's arguments.
    virtual std::unique_ptr<req_visitor_intf>
    visit_req_arg(
        std::size_t ix, std::unique_ptr<request_essentials> essentials)
        = 0;
};

/*
 * Resolving a request requires a context. A context may provide several modes
 * for resolving:
 * - Remote or local. A context implementation may support one or both.
 *   - It supports remote resolving if it implements remote_context_intf.
 *   - It supports local resolving if it implements local_context_intf.
 * - Sync or async. A context implementation may support one or both.
 *   - It supports synchronous resolving if it implements sync_context_intf.
 *   - It supports asynchronous resolving if it implements async_context_intf.
 * - Caching or non-caching. A cached request can be resolved only with a
 *   context supporting caching resolution, and such a context can also handle
 *   uncached requests.
 *   A context supports caching resolving if it implements
 *   caching_context_intf.
 * - Introspective or not. An introspective request can be resolved only with a
 *   context offering that support, and such a context can also handle
 *   non-introspective requests.
 *   A context supports introspective resolving if it implements
 *   introspective_context_intf.
 */

/*
 * Generic context interface
 */
class local_context_intf;
class remote_context_intf;
class sync_context_intf;
class async_context_intf;
class local_async_context_intf;
class root_local_async_context_intf;
class remote_async_context_intf;
class caching_context_intf;
class introspective_context_intf;

class context_intf
{
 public:
    virtual ~context_intf() = default;

    // Functions to cast to each context_intf derived class defined in this
    // file, avoiding expensive dynamic_cast's for these special cases.
    virtual local_context_intf*
    to_local_context_intf()
    {
        return nullptr;
    }
    virtual remote_context_intf*
    to_remote_context_intf()
    {
        return nullptr;
    }
    virtual sync_context_intf*
    to_sync_context_intf()
    {
        return nullptr;
    }
    virtual async_context_intf*
    to_async_context_intf()
    {
        return nullptr;
    }
    virtual local_async_context_intf*
    to_local_async_context_intf()
    {
        return nullptr;
    }
    virtual root_local_async_context_intf*
    to_root_local_async_context_intf()
    {
        return nullptr;
    }
    virtual remote_async_context_intf*
    to_remote_async_context_intf()
    {
        return nullptr;
    }
    virtual caching_context_intf*
    to_caching_context_intf()
    {
        return nullptr;
    }
    virtual introspective_context_intf*
    to_introspective_context_intf()
    {
        return nullptr;
    }

    // Returns the resources available for resolving a request.
    virtual inner_resources&
    get_resources()
        = 0;

    // Indicates if requests will be resolved remotely.
    // - Will return true if the context does not support local resolution
    // - Will return false if the context does not support remote resolution
    virtual bool
    remotely() const
        = 0;

    // Indicates if requests will be resolved asynchronously.
    // - Will return true if the context does not support synchronous
    //   resolution
    // - Will return false if the context does not support asynchronous
    //   resolution
    virtual bool
    is_async() const
        = 0;

    // The domain name identifies a context implementation class.
    // It is mainly used when a request is resolved remotely.
    virtual std::string const&
    domain_name() const
        = 0;

    // Delays the calling coroutine for the specified duration.
    // Cancellable if the context supports that.
    virtual cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) = 0;
};

/*
 * A context class implementing this interface declares that it supports
 * locally resolving requests.
 * Requests will still be resolved remotely if the class also implements
 * remote_context_intf, and remotely() returns true.
 */
class local_context_intf : public virtual context_intf
{
 public:
    virtual ~local_context_intf() = default;

    local_context_intf*
    to_local_context_intf() override
    {
        return this;
    }

    // A request function _must_ call this for creating a blob using shared
    // memory, and _should_ call it otherwise.
    // The returned object has a non-throwing data() implementation.
    // TODO split APIs into user vs. framework-internal parts
    virtual std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) = 0;

    // Intended for an (rpclib) server, which must call this function
    // immediately after creating the context object. The effect is that the
    // context will track all shared memory regions allocated via
    // make_data_owner(), such that they can be properly flushed via
    // on_value_complete().
    virtual void
    track_blob_file_writers()
        = 0;

    // Intended for an (rpclib) server, which must call this function before
    // sending a request resolution result back to the client. The function
    // flushes any shared memory regions allocated during the resolution.
    // Note that the rpclib server creates a dedicated context object for each
    // request resolution.
    virtual void
    on_value_complete()
        = 0;
};

/*
 * A context class implementing this interface declares that it supports
 * remotely resolving requests.
 * Requests will still be resolved locally if the class also implements
 * local_context_intf, and remotely() returns false.
 *
 * Remotely resolving a request means that the request is serialized, then
 * sent to the server; the response arrives from the server in some
 * serialization format, so needs to be deserialized.
 * Serialization and deserialization functions are template functions,
 * provided by a plugin. #include'ing the corresponding header file
 * activates the plugin; there is no runtime dispatching.
 */
class remote_context_intf : public virtual context_intf
{
 public:
    virtual ~remote_context_intf() = default;

    remote_context_intf*
    to_remote_context_intf() override
    {
        return this;
    }

    // The proxy that forwards requests to a remote executioner.
    // Throws when the proxy (name) was not registered.
    virtual remote_proxy&
    get_proxy() const
        = 0;

    // Creates the configuration to be passed to the remote.
    // need_record_lock is copied to the NEED_RECORD_LOCK config value.
    virtual service_config
    make_config(bool need_record_lock) const
        = 0;
};

// A context class implementing this interface declares that it supports
// synchronously resolving requests.
class sync_context_intf : public virtual context_intf
{
 public:
    virtual ~sync_context_intf() = default;

    sync_context_intf*
    to_sync_context_intf() override
    {
        return this;
    }
};

// Context that can be used for synchronously resolving requests; at least
// locally, in addition remotely if the actual context class also implements
// remote_context_intf.
class local_sync_context_intf : public virtual local_context_intf,
                                public virtual sync_context_intf
{
};

// Thrown when an asynchronous request resolution is cancelled
class async_cancelled : public std::runtime_error
{
    using runtime_error::runtime_error;
};

// Thrown when an asynchronous request resolution failed
class async_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

// Context for an asynchronously operating task (coroutine).
// One context object will be created for each task (coroutine);
// these objects form a tree with the same topology as the request tree.
// Thus, a context object tracks progress for a single (sub)request.
//
// The implication is that in general a context tree cannot be reused across
// resolve_request() calls. The exception is when retrying a resolve operation,
// where reuse is allowed and even desirable, as successful subresults from a
// former attempt could potentially be reused, avoiding re-resolution of that
// part of the request tree.
//
// Some get_...() functions are coroutines, others are not. The distinction is
// based on assumptions about the information always being available, or only
// after some time. These assumptions may no longer hold (e.g. if retriers are
// used).
// TODO consider revising context_intf::get_...() being coro or not
class async_context_intf : public virtual context_intf
{
 public:
    virtual ~async_context_intf() = default;

    async_context_intf*
    to_async_context_intf() override
    {
        return this;
    }

    // Gets a unique id for this task
    virtual async_id
    get_id() const
        = 0;

    // Returns true for a request, false for a plain value
    virtual bool
    is_req() const
        = 0;

    // Returns the number of subtasks.
    // If this context is a remote one and the requested information has not
    // yet been retrieved from the server, this call will populate the
    // context subtree, and block while that is happening. The current
    // implementation uses no coroutines, so this function isn't one either.
    virtual std::size_t
    get_num_subs() const
        = 0;

    // Gets the context for the subtask corresponding to the ix'th
    // subrequest (ix=0 representing the first subrequest).
    virtual async_context_intf&
    get_sub(std::size_t ix)
        = 0;

    // Gets the status of this task.
    // This is a coroutine version of local_async_context_intf::get_status(),
    // and is also available on remote-only contexts.
    virtual cppcoro::task<async_status>
    get_status_coro() = 0;

    // Requests cancellation of all tasks in the same context tree.
    // This is a coroutine version of
    // local_async_context_intf::request_cancellation(),
    // and is also available on remote-only contexts.
    virtual cppcoro::task<void>
    request_cancellation_coro() = 0;
};

// Context for an asynchronous task running on the local machine
class local_async_context_intf : public virtual local_context_intf,
                                 public virtual async_context_intf
{
 public:
    virtual ~local_async_context_intf() = default;

    local_async_context_intf*
    to_local_async_context_intf() override
    {
        return is_async() ? this : nullptr;
    }

    // For a root context, the essentials are set when a request is (first)
    // resolved using the context. For a non-root context, the essentials will
    // be passed to its constructor.
    virtual void
    set_essentials(std::unique_ptr<request_essentials> essentials)
        = 0;

    // Returns the number of subtasks
    // Differs from get_sub() by not being a coroutine.
    virtual std::size_t
    get_local_num_subs() const
        = 0;

    // Gets the context for the subtask corresponding to the ix'th
    // subrequest (ix=0 representing the first subrequest).
    // Differs from get_sub() in its return value.
    virtual local_async_context_intf&
    get_local_sub(std::size_t ix)
        = 0;

    // Reschedule execution for this context on another thread if this is
    // likely to improve performance due to increased parallelism.
    // Should be called only for real requests (is_req() returning true).
    // Should be called at most once per context.
    virtual cppcoro::task<void>
    reschedule_if_opportune() = 0;

    // Gets the status of this task.
    // This is a non-coroutine version of
    // async_context_intf::get_status_coro().
    virtual async_status
    get_status()
        = 0;

    // Gets the error message for this task.
    // Should be called only when get_status() returns ERROR.
    virtual std::string
    get_error_message()
        = 0;

    // Updates the status of this task.
    // If status is FINISHED or AWAITING_RESULT, also recursively updates
    // subtasks (needed if this task's result came from a cache).
    // If status == FINISHED and using_result() was called, the new status
    // will be AWAITING_RESULT.
    // TODO think of something less tricky for update_status()
    // TODO need to the the same if status == ERROR?
    // TODO keep history of an async request e.g.
    // TODO vector<tuple<async_status, timestamp>>
    // TODO plus extra info for ERROR status
    // TODO this could replace introspection
    virtual void
    update_status(async_status status)
        = 0;

    // Updates the status of this task to "ERROR", and stores an associated
    // error message.
    virtual void
    update_status_error(std::string const& errmsg)
        = 0;

    // Requests cancellation of all tasks in the same context tree.
    // This is a non-coroutine version of
    // async_context_intf::request_cancellation_coro().
    //
    // Note that after this call, tasks can still finish successfully or fail.
    // Thus, a "cancelling" state would not be meaningful.
    //
    // Also note that cancellation depends on cooperation by the request
    // implementation. In particular, an implementation that has no access to
    // the context object (such as a non-coroutine function_request) is unable
    // to cooperate. Thus, a cancellation request just may have no effect.
    virtual void
    request_cancellation()
        = 0;

    // Returns true if cancellation has been requested on this context or
    // another one in the same context tree.
    // The intention is that an asynchronous task will call this function on
    // polling basis, and call throw_async_cancelled() when it returns true.
    virtual bool
    is_cancellation_requested() const noexcept
        = 0;

    // Throws async_cancelled. Should be called (only) when
    // is_cancellation_requested() returns true.
    virtual void
    throw_async_cancelled() const
        = 0;

    // Sets a delegate that takes over the resolution of the request associated
    // with this context. The delegate will probably be a creq_context object:
    // a remote context, unlike the current local one.
    // Once a delegate has been set, it will stay, until it is deleted
    // (the current object need not own the delegate). It is not allowed to set
    // another delegate.
    // Relevant calls on the current context's interfaces should be forwarded
    // to the delegate; the most important ones probably are
    // request_cancellation_coro() and request_cancellation().
    virtual void
    set_delegate(std::shared_ptr<async_context_intf> delegate)
        = 0;

    // Returns the delegate, or null if it was not yet set, or deleted
    virtual std::shared_ptr<async_context_intf>
    get_delegate() = 0;
};

// Context for an asynchronous task running on the local machine that forms the
// root of a context tree.
class root_local_async_context_intf : public virtual local_async_context_intf
{
 public:
    root_local_async_context_intf*
    to_root_local_async_context_intf() override
    {
        return this;
    }

    // Returns a visitor that will traverse a request tree and build a
    // corresponding tree of subcontexts, under the current context object.
    virtual std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() = 0;

    // Calling this function indicates that the context will be used as
    // mailbox between a result producer (calling set_result()) and a result
    // consumer (calling get_result()). This should be done only for the root
    // context in a tree.
    virtual void
    using_result()
        = 0;

    // Sets the result of a finished task.
    // Should be called only when:
    // - using_result() was called before
    // - get_status() returns AWAITING_RESULT
    // Changes status to FINISHED.
    virtual void
    set_result(blob result)
        = 0;

    // Returns the value of a finished task.
    // Should be called only when:
    // - using_result() was called before
    // - get_status() returns FINISHED
    virtual blob
    get_result()
        = 0;

    // Sets the id of the memory cache record on the remote, if any, that was
    // locked while resolving the async request.
    // The id is stored here so that it can later be retrieved through a
    // get_cache_record_id() call.
    virtual void
    set_cache_record_id(remote_cache_record_id record_id)
        = 0;

    // Gets the id of the memory cache record on the remote, if any, that was
    // locked while resolving the async request.
    virtual remote_cache_record_id
    get_cache_record_id() const
        = 0;
};

// Context for an asynchronous task running on a (remote) server.
// This object will act as a proxy for a local_async_context_intf object on
// the server.
class remote_async_context_intf : public remote_context_intf,
                                  public virtual async_context_intf
{
 public:
    virtual ~remote_async_context_intf() = default;

    remote_async_context_intf*
    to_remote_async_context_intf() override
    {
        return this;
    }

    // Sets the id identifying this context on the remote server
    // (after this id has been retrieved from the server).
    virtual void
    set_remote_id(async_id remote_id)
        = 0;

    // Indicates that the remote id could not be retrieved.
    // Called from exception handler.
    virtual void
    fail_remote_id() noexcept
        = 0;

    // Returns the remote id, or NO_ASYNC_ID if it was not set.
    virtual async_id
    get_remote_id()
        = 0;

    // Make resolutions on this context introspective:
    // - Print out remote_id on submit_async()
    // - Keep actx tree on remote after resolution finished
    virtual void
    make_introspective()
        = 0;

    virtual bool
    introspective() const
        = 0;
};

// Context interface needed for locally resolving a cached request.
// Resources must provide at least a memory cache.
class caching_context_intf : public virtual local_context_intf
{
 public:
    virtual ~caching_context_intf() = default;

    caching_context_intf*
    to_caching_context_intf() override
    {
        return this;
    }
};

/*
 * Context interface needed for locally resolving an introspective request.
 * A context class implementing this interface should have a stack of
 * tasklet_tracker objects. It may get an initial push_tasklet() call when the
 * context is created. A local context will then get nested push_tasklet() /
 * pop_tasklet() calls during request resolution. These nested calls won't
 * happen during remote resolution, so a remote-only context class need
 * not implement this interface.
 */
class introspective_context_intf : public virtual context_intf
{
 public:
    virtual ~introspective_context_intf() = default;

    introspective_context_intf*
    to_introspective_context_intf() override
    {
        return this;
    }

    // Should return "most recent tasklet for this context" or nullptr
    virtual tasklet_tracker*
    get_tasklet()
        = 0;

    // Enter a nested introspection state
    virtual void
    push_tasklet(tasklet_tracker& tasklet)
        = 0;

    // Leave the current nested introspection state;
    // must match a preceding push_tasklet() call.
    virtual void
    pop_tasklet()
        = 0;
};

/*
 * A context that can be used for asynchronously / locally resolving more than
 * once; unlike a "normal" root_local_async_context_intf context, that can be
 * used for just one resolution.
 *
 * API for the framework; the client should interact with the context object
 * that implements this interface.
 */
class local_async_ctx_owner_intf : public virtual context_intf
{
 public:
    // Prepares this context for the first or next resolution.
    // Creates and returns the root async context object; sub-context objects
    // will e.g. be created by resolve_request().
    virtual root_local_async_context_intf&
    prepare_for_local_resolution()
        = 0;
};

/*
 * A context that can be used for asynchronously / remotely resolving more than
 * once; unlike a "normal" remote_async_context_intf context, that can be used
 * for just one resolution.
 *
 * API for the framework; the client should interact with the context object
 * that implements this interface.
 */
class remote_async_ctx_owner_intf : public virtual context_intf
{
 public:
    // Prepares this context for the first or next resolution.
    // Creates and returns the root async context object.
    virtual remote_async_context_intf&
    prepare_for_remote_resolution()
        = 0;
};

// The most generic/minimal context
template<typename Ctx>
concept Context = std::convertible_to<Ctx&, context_intf&>;

// Context that supports remote resolution
template<typename Ctx>
concept RemoteContext = std::convertible_to<Ctx&, remote_context_intf&>;

// Context that supports local resolution
template<typename Ctx>
concept LocalContext = std::convertible_to<Ctx&, local_context_intf&>;

// Context that supports remote resolution only, and does not support local
template<typename Ctx>
concept DefinitelyRemoteContext
    = std::is_final_v<Ctx> && RemoteContext<Ctx> && !
LocalContext<Ctx>;

// Context that supports local resolution only, and does not support remote
template<typename Ctx>
concept DefinitelyLocalContext = std::is_final_v<Ctx> && LocalContext<Ctx> && !
RemoteContext<Ctx>;

// Context that supports caching
template<typename Ctx>
concept CachingContext = std::convertible_to<Ctx&, caching_context_intf&>;

// Context that supports synchronous resolution
template<typename Ctx>
concept SyncContext = std::convertible_to<Ctx&, sync_context_intf&>;

// Context that supports asynchronous resolution
template<typename Ctx>
concept AsyncContext = std::convertible_to<Ctx&, async_context_intf&>;

// Context that supports synchronous resolution only, and does not support
// asynchronous
template<typename Ctx>
concept DefinitelySyncContext = std::is_final_v<Ctx> && SyncContext<Ctx> && !
AsyncContext<Ctx>;

// Context that supports asynchronous resolution only, and does not support
// synchronous
template<typename Ctx>
concept DefinitelyAsyncContext = std::is_final_v<Ctx> && AsyncContext<Ctx> && !
SyncContext<Ctx>;

// Context that supports introspection
template<typename Ctx>
concept IntrospectiveContext
    = std::convertible_to<Ctx&, introspective_context_intf&>;

// Any context implementation class should be valid
template<typename Ctx>
concept ValidContext
    = Context<Ctx>
      && (!std::is_final_v<Ctx> || RemoteContext<Ctx> || LocalContext<Ctx>)
      && (!std::is_final_v<Ctx> || SyncContext<Ctx> || AsyncContext<Ctx>);

// A valid final context implementation class
template<typename Ctx>
concept ValidFinalContext = Context<Ctx> && std::is_final_v<Ctx>
                            && (RemoteContext<Ctx> || LocalContext<Ctx>)
                            && (SyncContext<Ctx> || AsyncContext<Ctx>);

/*
 * A resolution retrier offers support for handling a failed resolution, by
 * retrying the resolution or not.
 *
 * If retryable is false, the resolution is never retried; otherwise, the
 * retrier must implement
 *   std::chrono::milliseconds
 *   handle_exception(int attempt, std::exception const& exc) const;
 * which must be called from an exception handler. The function either returns
 * the time to wait before the next resolution attempt, or throws if the
 * maximum number of attempts has been exceeded.
 */
template<typename T>
concept MaybeResolutionRetrier
    = std::same_as<std::remove_const_t<decltype(T::retryable)>, bool>;

template<typename T>
concept ResolutionRetrier
    = MaybeResolutionRetrier<T>
      && requires(T const& obj) {
             {
                 obj.handle_exception(0, std::runtime_error{""})
                 } -> std::convertible_to<std::chrono::milliseconds>;
         };

/*
 * A request is something that can be resolved, resulting in a result
 *
 * Compile-time attributes:
 * - value_type: result type
 * - is_proxy: indicates whether this is a proxy request
 * - retryable: indicates whether a failing resolution can be retried
 *
 * Runtime-time attributes:
 * - caching_level
 * - introspective: indicates this request's wish to have its resolution be
 *   introspected
 * - introspection_title: title to use in introspection output, valid if
 *   introspective
 * - essentials. May return nullptr (e.g. for value_request).
 *
 * TODO is_introspective+get_introspection_title vs get_essentials
 */
template<typename T>
concept Request
    = requires {
          requires std::same_as<typename T::element_type, T>;
          typename T::value_type;
          requires std::
              same_as<std::remove_const_t<decltype(T::is_proxy)>, bool>;
          requires std::
              same_as<std::remove_const_t<decltype(T::retryable)>, bool>;
      } && requires(T const& req) {
               {
                   req.get_caching_level()
                   } -> std::same_as<caching_level_type>;
           } && requires(T const& req) {
                    {
                        req.is_introspective()
                        } -> std::same_as<bool>;
                } && requires(T const& req) {
                         {
                             req.get_introspection_title()
                             } -> std::same_as<std::string>;
                     } && requires(T const& req) {
                              {
                                  req.get_essentials()
                                  } -> std::same_as<
                                      std::unique_ptr<request_essentials>>;
                          };

// By having retryable=true, a request advertises itself as being retryable...
template<typename Req>
concept RetryableRequest = Request<Req> && Req::retryable;
// ... but it also needs to implement the corresponding function.
template<typename Req>
concept ValidRetryableRequest
    = RetryableRequest<Req> && ResolutionRetrier<Req>;

// A request that accepts visitors. A visitor will recursively visit all
// subrequests, so all of these should be visitable as well.
// This functionality is being used for constructing a context tree when a
// request is resolved locally and asynchronously.
template<typename Req>
concept VisitableRequest
    = Request<Req> && requires(Req const& req) {
                          {
                              req.accept(*(req_visitor_intf*) nullptr)
                          };
                      };

// A context/request pair where the context can be used to resolve the request.
template<typename Ctx, typename Req>
concept MatchingContextRequest = Request<Req> && Context<Ctx>;

// Contains the type of an argument to an rq_function-like call.
template<typename Value, bool IsReq>
struct arg_type_struct;

// Non-request argument: type is that of the argument itself.
template<typename Arg>
struct arg_type_struct<Arg, false>
{
    using value_type = Arg;
};

// Request argument: type is that of the request's return value.
template<typename Arg>
struct arg_type_struct<Arg, true>
{
    using value_type = typename Arg::value_type;
};

// Yields the type of an argument to an rq_function-like call
template<typename T>
using arg_type =
    typename arg_type_struct<std::decay_t<T>, Request<std::decay_t<T>>>::
        value_type;

} // namespace cradle

#endif
