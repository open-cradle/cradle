#ifndef CRADLE_INNER_REQUESTS_GENERIC_H
#define CRADLE_INNER_REQUESTS_GENERIC_H

#include <concepts>
#include <memory>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/requests/types.h>
#include <cradle/inner/service/config.h>

namespace cradle {

struct immutable_cache;
class inner_resources;
class request_uuid;
class tasklet_tracker;

// Specifies how request resolution results should be cached
enum class caching_level_type
{
    none, // No caching
    memory, // Caching in local memory only
    full // Caching in local memory, plus some secondary storage
};

// Disallow relational comparisons between caching_level_type values.
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
    return static_cast<int>(level)
           == static_cast<int>(caching_level_type::memory);
}

inline constexpr bool
is_fully_cached(caching_level_type level)
{
    return static_cast<int>(level)
           == static_cast<int>(caching_level_type::full);
}

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
    visit_req_arg(std::size_t ix) = 0;
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

    // The framework must call this once the value has been completely
    // resolved. It will flush any shared memory regions allocated during the
    // resolution.
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

    // The proxy name identifies the proxy that will forward requests to a
    // remote executioner.
    virtual std::string const&
    proxy_name() const
        = 0;

    // The proxy itself. Will throw when it was not registered.
    virtual remote_proxy&
    get_proxy() const
        = 0;

    // The domain name identifies a context implementation class.
    virtual std::string const&
    domain_name() const
        = 0;

    // Creates the configuration to be passed to the remote.
    virtual service_config
    make_config() const
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
class local_async_context_intf : public local_context_intf,
                                 public async_context_intf
{
 public:
    virtual ~local_async_context_intf() = default;

    local_async_context_intf*
    to_local_async_context_intf() override
    {
        return this;
    }

    // Some redundant redefinitions to prevent MSVC C4250
    local_context_intf*
    to_local_context_intf() override
    {
        return this;
    }
    async_context_intf*
    to_async_context_intf() override
    {
        return this;
    }

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

    // Returns a visitor that will traverse a request tree and build a
    // corresponding tree of subcontexts, under the current context object.
    // Should be called only for a root context (a context that forms the
    // root of its context tree).
    virtual std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() = 0;

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
    // If status == FINISHED, also recursively updates subtasks
    // (needed if this task's result came from a cache).
    // If status == FINISHED and using_result() was called, the new status
    // will be AWAITING_RESULT.
    // status must not be AWAITING_RESULT
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

    // Requests cancellation of all tasks in the same context tree.
    // This is a non-coroutine version of
    // async_context_intf::request_cancellation().
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
    // The intention is that an asynchronous task will all this function on
    // polling basis, and call throw_async_cancelled() when it returns true.
    virtual bool
    is_cancellation_requested() const noexcept
        = 0;

    // Throws async_cancelled. Should be called (only) when
    // is_cancellation_requested() returns true.
    virtual void
    throw_async_cancelled() const
        = 0;
};

// Context for an asynchronous task running on a (remote) server.
// This object will act as a proxy for a local_async_context_intf object on
// the server.
class remote_async_context_intf : public remote_context_intf,
                                  public async_context_intf
{
 public:
    virtual ~remote_async_context_intf() = default;

    remote_async_context_intf*
    to_remote_async_context_intf() override
    {
        return this;
    }

    // Some redundant redefinitions to prevent MSVC C4250
    remote_context_intf*
    to_remote_context_intf() override
    {
        return this;
    }
    async_context_intf*
    to_async_context_intf() override
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
};

// Context interface needed for resolving a cached request.
// Implicitly local-only although not derived from local_context_intf,
// but an implementation class should do that.
// Resources must provide at least a memory cache.
class caching_context_intf : public virtual context_intf
{
 public:
    virtual ~caching_context_intf() = default;

    caching_context_intf*
    to_caching_context_intf() override
    {
        return this;
    }
};

// Context interface needed for resolving an introspective request.
// Implicitly local-only although not derived from local_context_intf,
// but an implementation class should do that.
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

    virtual void
    push_tasklet(tasklet_tracker& tasklet)
        = 0;

    // Must match a preceding push_tasklet() call
    virtual void
    pop_tasklet()
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
concept LocalContext
    = std::convertible_to<Ctx&, local_context_intf&>
      || std::convertible_to<Ctx&, local_async_context_intf&>
      || std::convertible_to<Ctx&, caching_context_intf&>
      || std::convertible_to<Ctx&, introspective_context_intf&>;

// Context that supports remote resolution only, and does not support local
template<typename Ctx>
concept DefinitelyRemoteContext
    = std::is_final_v<Ctx> && RemoteContext<Ctx> && !
LocalContext<Ctx>;

// Context that supports local resolution only, and does not support remote
template<typename Ctx>
concept DefinitelyLocalContext = std::is_final_v<Ctx> && LocalContext<Ctx> && !
RemoteContext<Ctx>;

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

// Any context implementation class should be valid
template<typename Ctx>
concept ValidContext
    = Context<Ctx>
      && (!std::is_final_v<Ctx> || RemoteContext<Ctx> || LocalContext<Ctx>)
      && (!std::is_final_v<Ctx> || SyncContext<Ctx> || AsyncContext<Ctx>);

/*
 * A request is something that can be resolved, resulting in a result
 *
 * Attributes:
 * - value_type: result type
 * - caching_level
 * - a boolean indicating whether this is a proxy request
 * - introspective
 * - An id uniquely identifying the request (class). Can be a placeholder
 *   if such identification is not needed.
 *
 * TODO make request's value restrictions explicit
 * TODO define when request's id can be a placeholder
 */
template<typename T>
concept Request
    = requires {
          requires std::same_as<typename T::element_type, T>;
          typename T::value_type;
          requires std::same_as<
              std::remove_const_t<decltype(T::caching_level)>,
              caching_level_type>;
          requires std::
              same_as<std::remove_const_t<decltype(T::is_proxy)>, bool>;
          requires std::
              same_as<std::remove_const_t<decltype(T::introspective)>, bool>;
      };
// TODO say something about resolve_sync()/_async() as we used to do

template<typename T>
concept UncachedRequest = Request<T> && is_uncached(T::caching_level);

template<typename Req>
concept CachedRequest = Request<Req> && is_cached(Req::caching_level)
                        && requires(Req const& req) {
                               {
                                   req.get_captured_id()
                                   }
                                   -> std::convertible_to<captured_id const&>;
                           };

template<typename Req>
concept MemoryCachedRequest
    = CachedRequest<Req> && is_memory_cached(Req::caching_level);

template<typename Req>
concept FullyCachedRequest
    = CachedRequest<Req> && is_fully_cached(Req::caching_level);

template<typename Req>
concept IntrospectiveRequest
    = Req::introspective && requires(Req const& req) {
                                {
                                    req.get_introspection_title()
                                    } -> std::convertible_to<std::string>;
                            };

template<typename Req>
concept NonIntrospectiveRequest = !
Req::introspective;

template<typename Req>
concept CachedIntrospectiveRequest
    = CachedRequest<Req> && IntrospectiveRequest<Req>;

template<typename Req>
concept CachedNonIntrospectiveRequest
    = CachedRequest<Req> && NonIntrospectiveRequest<Req>;

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

// Contains the type of an argument to an rq_function-like call.
// Primary template, used for non-request types; should be specialized
// for each kind of request (in other .h files).
template<typename T>
struct arg_type_struct
{
    using value_type = T;
};

// Yields the type of an argument to an rq_function-like call
template<typename T>
using arg_type = typename arg_type_struct<std::decay_t<T>>::value_type;

} // namespace cradle

#endif
