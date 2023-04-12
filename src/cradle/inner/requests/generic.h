#ifndef CRADLE_INNER_REQUESTS_GENERIC_H
#define CRADLE_INNER_REQUESTS_GENERIC_H

#include <concepts>
#include <memory>
#include <stdexcept>
#include <string>

#include <cppcoro/static_thread_pool.hpp>
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

// unique_ptr<Request> or shared_ptr<Request>
template<typename T>
concept RequestPtr = Request<typename T::element_type>;

template<typename T>
concept RequestOrPtr = Request<T> || RequestPtr<T>;

template<typename T>
concept UncachedRequest = Request<T> && T::caching_level ==
caching_level_type::none;

template<typename T>
concept UncachedRequestPtr = UncachedRequest<typename T::element_type>;

template<typename Req>
concept CachedRequest = Request<Req>&& Req::caching_level
                        != caching_level_type::none&& requires(Req const& req)
{
    {
        req.get_captured_id()
    }
    ->std::convertible_to<captured_id const&>;
};

template<typename Req>
concept MemoryCachedRequest
    = CachedRequest<Req>&& Req::caching_level == caching_level_type::memory;

template<typename Req>
concept FullyCachedRequest
    = CachedRequest<Req>&& Req::caching_level == caching_level_type::full;

template<typename Req>
concept IntrospectiveRequest = Req::introspective&& requires(Req const& req)
{
    {
        req.get_introspection_title()
    }
    ->std::convertible_to<std::string>;
};

template<typename Req>
concept NonIntrospectiveRequest = !Req::introspective;

template<typename Req>
concept CachedIntrospectiveRequest
    = CachedRequest<Req>&& IntrospectiveRequest<Req>;

template<typename Req>
concept CachedNonIntrospectiveRequest
    = CachedRequest<Req>&& NonIntrospectiveRequest<Req>;

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

// Generic context interface, can be used for resolving uncached
// non-introspective requests
class context_intf
{
 public:
    virtual ~context_intf() = default;

    // Indicates if non-trivial requests should be resolved remotely
    // Should return true only if this is a remotable_context_intf
    virtual bool
    remotely() const = 0;
};

// Context interface needed for resolving a cached request
class cached_context_intf : public virtual context_intf
{
 public:
    virtual ~cached_context_intf() = default;

    virtual inner_resources&
    get_resources()
        = 0;

    virtual immutable_cache&
    get_cache()
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
class cached_introspective_context_intf : public cached_context_intf,
                                          public introspective_context_intf
{
 public:
    virtual ~cached_introspective_context_intf() = default;
};

// Context interface needed for remotely resolving requests
// Requests will still be resolved locally if remotely() returns false.
class remote_context_intf : public virtual context_intf
{
 public:
    virtual ~remote_context_intf() = default;

    virtual std::string const&
    proxy_name() const = 0;

    virtual std::string const&
    domain_name() const = 0;

    // Creates a clone of *this that differs only in its remotely() returning
    // false
    virtual std::shared_ptr<remote_context_intf>
    local_clone() const = 0;

    // TODO return request serialization method
    // TODO return response serialization method
};

// If ctx is remote, convert to remote_context_intf
// otherwise, return nullptr
remote_context_intf*
to_remote_context_intf(context_intf& ctx);

// Status of an asynchronous operation: a (cppcoro) task, associated
// with a coroutine
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

// Context for an asynchronously operating coroutine.
// Fine-grained: one object will be created for each task (coro);
// these objects form a tree with the same topology as the request tree.
class async_context_intf : public virtual context_intf
{
 public:
    virtual ~async_context_intf() = default;

    // Gets a unique id for this task
    virtual async_id
    get_id() const = 0;

    // Returns true for a request, false for a plain value
    virtual bool
    is_req() const = 0;

    // Returns the number of subtasks
    virtual std::size_t
    get_num_subs() const = 0;

    // Gets the status of this task
    virtual cppcoro::task<async_status>
    get_status_coro() = 0;

    // Requests cancellation of all tasks in the same context tree
    virtual cppcoro::task<void>
    request_cancellation_coro() = 0;
};

// Context for an asynchronous task running on the local machine
class local_async_context_intf : public async_context_intf
{
 public:
    virtual ~local_async_context_intf() = default;

    // Get the context for the subtask corresponding to the ix'th
    // subrequest (ix=0 representing the first subrequest)
    virtual local_async_context_intf&
    get_sub(std::size_t ix)
        = 0;

    // Should be called only for a root context.
    // Returns a visitor that will traverse a request tree and build a
    // corresponding tree of subcontexts, under the current context object.
    virtual std::unique_ptr<req_visitor_intf>
    make_ctx_tree_builder() = 0;

    // Returns a hint indicating whether rescheduling execution for this
    // context on another thread might improve performance due to increased
    // parallelism.
    // Should be called only for real requests (is_req() returning true).
    // Should be called only once per context (i.e., may have side effects).
    virtual bool
    decide_reschedule()
        = 0;

    // Gets the status of this task
    virtual async_status
    get_status()
        = 0;

    // Gets the error message for this task
    // Should be called only when get_status() returns ERROR
    virtual std::string
    get_error_message()
        = 0;

    // Updates the status of this task
    // If status == FINISHED, also recursively updates subtasks
    // (needed if this task's result came from a cache)
    // TODO need to the the same if status == ERROR?
    // TODO keep history of an async request e.g.
    // TODO vector<tuple<async_status, timestamp>>
    // TODO plus extra info for ERROR status
    // TODO this could replace introspection
    virtual void
    update_status(async_status status)
        = 0;

    // Updates the status of this task to "ERROR", and store an associated
    // error message
    virtual void
    update_status_error(std::string const& errmsg)
        = 0;

    // Returns a thread pool that can be used by a caller
    virtual cppcoro::static_thread_pool&
    get_thread_pool()
        = 0;

    // Sets the result of a finished task
    virtual void
    set_result(blob result)
        = 0;

    // Returns the value of a finished task
    virtual blob
    get_result()
        = 0;

    // Requests cancellation of all tasks in the same context tree
    virtual void
    request_cancellation()
        = 0;

    // Returns true if cancellation has been requested on this context or
    // another one in the same context tree
    virtual bool
    is_cancellation_requested() const noexcept = 0;

    // Throws async_cancelled if cancellation was requested
    virtual void
    throw_if_cancellation_requested() const = 0;
};

// Context for an asynchronous task running on a (remote) server.
// This object will act as a proxy for a local_async_context_intf object on
// the server.
class remote_async_context_intf : public async_context_intf,
                                  public remote_context_intf
{
 public:
    virtual ~remote_async_context_intf() = default;

    // Creates a "local" counterpart of this object
    virtual std::shared_ptr<local_async_context_intf>
    local_async_clone() const = 0;

    virtual remote_async_context_intf&
    add_sub(async_id sub_remote_id, bool is_req)
        = 0;

    // Get the context for the subtask corresponding to the ix'th
    // subrequest (ix=0 representing the first subrequest)
    virtual remote_async_context_intf&
    get_sub(std::size_t ix)
        = 0;

    virtual void
    set_remote_id(async_id remote_id)
        = 0;

    // Returns the remote id.
    // Should be called only if the remote id has been set (by the caller,
    // presumably).
    virtual async_id
    get_remote_id()
        = 0;

    // Returns true if cancellation was requested before the remote_id became
    // available.
    // The caller should forward the request to the server.
    virtual bool
    cancellation_pending()
        = 0;
};

// Convert ctx to local_async_context_intf*, or return nullptr if the
// runtime type doesn't match.
local_async_context_intf*
to_local_async_context_intf(context_intf& ctx);

// Converts ctx if it is a local_async_context_intf*, or returns an empty
// shared_ptr if the runtime type doesn't match.
std::shared_ptr<local_async_context_intf>
to_local_async_context_intf(std::shared_ptr<context_intf> const& ctx);

// Convert ctx to remote_async_context_intf*, or return nullptr if the
// runtime type doesn't match.
remote_async_context_intf*
to_remote_async_context_intf(context_intf& ctx);

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
    using type = context_intf;
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
using ctx_type_for_req = typename ctx_type_helper<Req::introspective, Req::caching_level>::type;

// The most generic/minimal context
template<typename Ctx>
concept Context = std::convertible_to<Ctx&, context_intf&>;

// Context Ctx can be used for resolving a request characterized by Intrsp and
// Level
template<typename Ctx, bool Intrsp, caching_level_type Level>
concept ContextMatchingProps
    = std::convertible_to<Ctx&, ctx_type_for_props<Intrsp, Level>&>;

// Context Ctx can be used for resolving request Req
template<typename Ctx, typename Req>
concept ContextMatchingRequest
    = std::convertible_to<Ctx&, ctx_type_for_req<Req>&>&& requires(
        Req const& req, Ctx& ctx)
{
    {
        req.resolve(ctx)
    }
    ->std::same_as<cppcoro::task<typename Req::value_type>>;
};

template<typename Ctx>
concept AsyncContext = std::convertible_to<Ctx&, async_context_intf&>;

template<typename Ctx>
concept LocalAsyncContext
    = std::convertible_to<Ctx&, local_async_context_intf&>;

template<typename Ctx>
concept RemoteAsyncContext
    = std::convertible_to<Ctx&, remote_async_context_intf&>;

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

} // namespace cradle

#endif
