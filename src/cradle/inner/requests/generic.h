#ifndef CRADLE_INNER_REQUESTS_GENERIC_H
#define CRADLE_INNER_REQUESTS_GENERIC_H

#include <concepts>
#include <memory>
#include <stdexcept>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>
#include <cradle/inner/core/type_definitions.h>

namespace cradle {

struct immutable_cache;
class inner_resources;
class request_uuid;
class tasklet_tracker;

enum class caching_level_type
{
    none,
    memory,
    full
};

template<typename T>
concept Request
    = requires {
          std::same_as<typename T::element_type, T>;
          typename T::value_type;
          std::same_as<decltype(T::caching_level), caching_level_type>;
          std::same_as<decltype(T::introspective), bool>;
      } && requires(T const& req) {
               {
                   req.get_uuid()
                   } -> std::convertible_to<request_uuid>;
           };

template<typename T>
concept UncachedRequest = Request<T> && T::caching_level ==
caching_level_type::none;

template<typename Req>
concept CachedRequest = Request<Req> && Req::caching_level !=
caching_level_type::none&& requires(Req const& req) {
                               {
                                   req.get_captured_id()
                                   }
                                   -> std::convertible_to<captured_id const&>;
                           };

template<typename Req>
concept MemoryCachedRequest = CachedRequest<Req> && Req::caching_level ==
caching_level_type::memory;

template<typename Req>
concept FullyCachedRequest = CachedRequest<Req> && Req::caching_level ==
caching_level_type::full;

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
using arg_type = typename arg_type_struct<T>::value_type;

// Visits a request, and its arguments (which may be subrequests themselves).
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
 * - Cached or uncached. A cached request can be resolved only with a context
 *   supporting cached resolution, and such a context can also handle uncached
 *   requests.
 *   A context supports cached resolving if it implements cached_context_intf.
 * - Introspective or not. An introspective request can be resolved only with a
 *   context offering that support, and such a context can also handle
 *   non-introspective requests.
 *   A context supports introspective resolving if it implements
 *   introspective_context_intf.
 */

/*
 * Generic context interface
 */
class context_intf
{
 public:
    virtual ~context_intf() = default;

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

    // The proxy name identifies the proxy that will forward requests to a
    // remote executioner.
    virtual std::string const&
    proxy_name() const
        = 0;

    // The domain name identifies a context implementation class.
    virtual std::string const&
    domain_name() const
        = 0;

    // Creates a variant of *this that can be used to locally resolve the same
    // set of requests. In particular, the new object's remotely() will return
    // false.
    // Will be called only when *this is the root of a context tree.
    // TODO formalize "context object is root"
    // Useful for a loopback service.
    virtual std::shared_ptr<local_context_intf>
    local_clone() const = 0;
};

// A context class implementing this interface declares that it supports
// synchronously resolving requests.
class sync_context_intf : public virtual context_intf
{
 public:
    virtual ~sync_context_intf() = default;
};

// Status of an asynchronous operation: a (cppcoro) task, associated
// with a coroutine.
// CANCELLED, FINISHED and ERROR are final statuses: once a task ends up in
// one of these, its status won't change anymore.
enum class async_status
{
    CREATED, // Task was created
    SUBS_RUNNING, // Subtasks running, main task waiting for them
    SELF_RUNNING, // Subtasks finished, main task running
    CANCELLING, // Detected cancellation request
    CANCELLED, // Cancellation completed
    FINISHED, // Finished successfully
    ERROR // Ended due to error
};

std::string
to_string(async_status s);

// Identifies an async operation. Unique within the context of its
// (local or remote) service.
using async_id = uint64_t;

static constexpr async_id NO_ASYNC_ID{~async_id{}};

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

    // Gets a unique id for this task
    virtual async_id
    get_id() const
        = 0;

    // Returns true for a request, false for a plain value
    virtual bool
    is_req() const
        = 0;

    // Returns the number of subtasks
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
    // This is a non-coroutine version of async_context_intf::get_status().
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

    // Sets the result of a finished task.
    // Should be called only when get_status() returns FINISHED.
    virtual void
    set_result(blob result)
        = 0;

    // Returns the value of a finished task.
    // Should be called only when get_status() returns FINISHED.
    virtual blob
    get_result()
        = 0;

    // Requests cancellation of all tasks in the same context tree
    // This is a non-coroutine version of
    // async_context_intf::request_cancellation().
    virtual void
    request_cancellation()
        = 0;

    // Returns true if cancellation has been requested on this context or
    // another one in the same context tree.
    virtual bool
    is_cancellation_requested() const noexcept
        = 0;

    // Throws async_cancelled if cancellation was requested.
    // The intention is that an asynchronous task will all this function on
    // polling basis.
    virtual void
    throw_if_cancellation_requested() const
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

    // Creates a variant of *this that can be used to locally resolve the same
    // set of requests. In particular, the new object's remotely() will return
    // false.
    // Will be called only when *this is the root of a context tree.
    // Useful for a loopback service.
    virtual std::shared_ptr<local_async_context_intf>
    local_async_clone() const = 0;

    // Adds a subcontext (corresponding to a subrequest) to this one.
    virtual remote_async_context_intf&
    add_sub(bool is_req)
        = 0;

    // Gets the context for the subtask corresponding to the ix'th
    // subrequest (ix=0 representing the first subrequest).
    // Differs from get_sub() in its return value.
    virtual remote_async_context_intf&
    get_remote_sub(std::size_t ix)
        = 0;

    // Sets the id identifying this context on the remote server
    // (after this id has been retrieved from the server).
    virtual void
    set_remote_id(async_id remote_id)
        = 0;

    // Returns the remote id, or NO_ASYNC_ID if it was not set.
    virtual async_id
    get_remote_id()
        = 0;

    // Returns true if cancellation was requested before the root remote_id
    // became available.
    // The caller is responsible to call this function once it has obtained
    // the root remote_id, and if it returns true, call request_cancellation().
    virtual bool
    cancellation_pending()
        = 0;
};

// Context interface needed for resolving a cached request
class cached_context_intf : public virtual context_intf
{
 public:
    virtual ~cached_context_intf() = default;

    // Returns resources implementing the cache(s).
    virtual inner_resources&
    get_resources()
        = 0;
};

// Context interface needed for resolving an introspective request
class introspective_context_intf : public virtual context_intf
{
 public:
    virtual ~introspective_context_intf() = default;

    virtual tasklet_tracker*
    get_tasklet()
        = 0;

    virtual void
    push_tasklet(tasklet_tracker* tasklet)
        = 0;

    virtual void
    pop_tasklet()
        = 0;
};

// Context interface needed for resolving a request that is both cached and
// introspective
// A context class that is both caching and introspective should implement this
// interface, rather than the two individual ones. The reason is that
// ctx_type_for_props needs a single type.
class cached_introspective_context_intf : public cached_context_intf,
                                          public introspective_context_intf
{
 public:
    virtual ~cached_introspective_context_intf() = default;
};

// The most generic/minimal context
template<typename Ctx>
concept Context = std::convertible_to<Ctx&, context_intf&>;

// Context that supports remote resolution only, and does not support local
template<typename Ctx>
concept DefinitelyRemoteContext
    = std::is_final_v<Ctx> && std::convertible_to<Ctx&, remote_context_intf&>
      && !
std::convertible_to<Ctx&, local_context_intf&>;

// Context that supports local resolution only, and does not support remote
template<typename Ctx>
concept DefinitelyLocalContext
    = std::is_final_v<Ctx> && std::convertible_to<Ctx&, local_context_intf&>
      && !
std::convertible_to<Ctx&, remote_context_intf&>;

template<typename Ctx>
concept AsyncContext = std::convertible_to<Ctx&, async_context_intf&>;

template<typename Ctx>
concept DefinitelyAsyncContext
    = std::is_final_v<Ctx> && std::convertible_to<Ctx&, async_context_intf&>
      && !
std::convertible_to<Ctx&, sync_context_intf&>;

template<typename Ctx>
concept DefinitelySyncContext
    = std::is_final_v<Ctx> && std::convertible_to<Ctx&, sync_context_intf&>
      && !
std::convertible_to<Ctx&, async_context_intf&>;

template<typename Ctx>
concept LocalAsyncContext
    = std::convertible_to<Ctx&, local_async_context_intf&>;

template<typename Ctx>
concept RemoteAsyncContext
    = std::convertible_to<Ctx&, remote_async_context_intf&>;

// A context that supports caching, which will happen when the request demands
// it
template<typename Ctx>
concept CachingContext = std::convertible_to<Ctx&, cached_context_intf&>;

// A context that supports introspection, which will happen when the request
// demands it
template<typename Ctx>
concept IntrospectiveContext
    = std::convertible_to<Ctx&, introspective_context_intf&>;

// Any context implementation class should be valid
template<typename Ctx>
concept ValidContext
    = Context<Ctx>
      && (!std::is_final_v<Ctx>
          || std::convertible_to<Ctx&, remote_context_intf&>
          || std::convertible_to<Ctx&, local_context_intf&>)
      && (!std::is_final_v<Ctx>
          || std::convertible_to<Ctx&, sync_context_intf&>
          || std::convertible_to<Ctx&, async_context_intf&>)
      && (!std::is_final_v<Ctx> || !CachingContext<Ctx>
          || !IntrospectiveContext<Ctx>
          || std::convertible_to<Ctx&, cached_introspective_context_intf&>);

// Defines the context_intf derived class needed for resolving a request
// characterized by Intrsp and Level.
// Primary template first, followed by the four partial specializations for
// all (is request introspective?, is request cached?) combinations.
template<bool Intrsp, caching_level_type Level>
struct ctx_type_helper;

template<caching_level_type Level>
struct ctx_type_helper<false, Level>
{
    // Resolving a cached function request requires a cached context.
    using type = cached_context_intf;
};

template<caching_level_type Level>
struct ctx_type_helper<true, Level>
{
    // Resolving a cached+introspective function request requires a
    // cached+introspective context.
    using type = cached_introspective_context_intf;
};

template<>
struct ctx_type_helper<false, caching_level_type::none>
{
    // An uncached function request can be resolved using any kind of context.
    using type = local_context_intf;
};

template<>
struct ctx_type_helper<true, caching_level_type::none>
{
    // Resolving an introspective function request requires an introspective
    // context.
    using type = introspective_context_intf;
};

// The minimal context type needed for resolving a request characterized by
// Intrsp and Level
template<bool Intrsp, caching_level_type Level>
using ctx_type_for_props = typename ctx_type_helper<Intrsp, Level>::type;

// The minimal context type needed for resolving request Req
template<Request Req>
using ctx_type_for_req =
    typename ctx_type_helper<Req::introspective, Req::caching_level>::type;

// Context Ctx can be used for locally resolving a request characterized by
// Intrsp and Level
template<typename Ctx, bool Intrsp, caching_level_type Level>
concept ContextMatchingProps
    = Context<Ctx>
      && (Level == caching_level_type::none || CachingContext<Ctx>)
      && (!Intrsp || IntrospectiveContext<Ctx>);

// Context Ctx can be used for locally resolving request Req
template<typename Ctx, typename Req>
concept ContextMatchingRequest
    = Context<Ctx> && Request<Req>
      && (Req::caching_level == caching_level_type::none
          || CachingContext<Ctx>)
      && (!Req::introspective || IntrospectiveContext<Ctx>)
      && requires(Req const& req, Ctx& ctx) {
             {
                 req.resolve(ctx)
                 } -> std::same_as<cppcoro::task<typename Req::value_type>>;
         };

// tasklet_tracker context, ending when the destructor is called
class tasklet_context
{
 public:
    tasklet_context(
        introspective_context_intf& ctx,
        std::string const& pool_name,
        std::string const& title);

    ~tasklet_context();

 private:
    introspective_context_intf* ctx_{nullptr};
};

// Casts ctx, or returns nullptr if it won't remotely resolve requests
remote_context_intf*
to_remote_ptr(context_intf& ctx);

// Casts ctx, or returns nullptr if it won't remotely and asynchronously
// resolve requests
remote_async_context_intf*
to_remote_async_ptr(context_intf& ctx);

// Casts ctx, or throws if it won't remotely and asynchronously resolve
// requests
remote_async_context_intf&
to_remote_async_ref(context_intf& ctx);

// Casts ctx, or throws if it won't locally resolve requests
local_context_intf&
to_local_ref(context_intf& ctx);

// Casts ctx, or throws if it won't locally and asynchronously resolve requests
local_async_context_intf&
to_local_async_ref(context_intf& ctx);

// Casts ctx, or throws if it won't locally and asynchronously resolve requests
std::shared_ptr<local_async_context_intf>
to_local_async(std::shared_ptr<context_intf> const& ctx);

} // namespace cradle

#endif
