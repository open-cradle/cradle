#ifndef CRADLE_INNER_REQUESTS_GENERIC_H
#define CRADLE_INNER_REQUESTS_GENERIC_H

#include <concepts>
#include <memory>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>

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
    virtual std::unique_ptr<remote_context_intf>
    local_clone() const = 0;

    // TODO return request serialization method
    // TODO return response serialization method
};

// If ctx is remote, convert to remote_context_intf
// otherwise, return nullptr
remote_context_intf*
to_remote_context_intf(context_intf& ctx);

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
