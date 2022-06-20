#ifndef CRADLE_INNER_REQUESTS_GENERIC_H
#define CRADLE_INNER_REQUESTS_GENERIC_H

#include <concepts>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>

namespace cradle {

enum class caching_level_type
{
    none,
    memory,
    full
};

template<typename T>
concept Request = requires
{
    std::same_as<typename T::element_type, T>;
    typename T::value_type;
    std::same_as<decltype(T::caching_level), caching_level_type>;
};

template<typename T>
concept UncachedRequest
    = Request<T>&& T::caching_level == caching_level_type::none;

template<typename T>
concept UncachedRequestPtr = UncachedRequest<typename T::element_type>;

template<typename T>
concept UncachedRequestOrPtr = UncachedRequest<T> || UncachedRequestPtr<T>;

template<typename Req>
concept CachedRequest = Request<Req>&& Req::caching_level
                        != caching_level_type::none&& requires(Req const& req)
{
    {
        req.get_captured_id()
    }
    ->std::convertible_to<captured_id const&>;
};

template<typename T>
concept CachedRequestPtr = CachedRequest<typename T::element_type>;

template<typename T>
concept CachedRequestOrPtr = CachedRequest<T> || CachedRequestPtr<T>;

template<typename T>
concept RequestContext = true;

template<typename T>
concept UncachedContext = true;

struct inner_service_core;

template<typename Ctx>
concept CachedContext = requires(Ctx& ctx)
{
    {
        ctx.get_service()
    }
    ->std::convertible_to<inner_service_core&>;
};

template<typename Ctx, typename Req>
concept MatchingContextRequest = requires(Req const& req, Ctx& ctx)
{
    {
        req.resolve(ctx)
    }
    ->std::same_as<cppcoro::task<typename Req::value_type>>;
};

template<typename Ctx, typename Req>
concept UncachedContextRequest = UncachedContext<Ctx>&& UncachedRequest<Req>&&
    MatchingContextRequest<Ctx, Req>;

template<typename Ctx, typename Req>
concept CachedContextRequest = CachedContext<Ctx>&& CachedRequest<Req>&&
    MatchingContextRequest<Ctx, Req>;

class uncached_context_intf
{
 public:
    virtual ~uncached_context_intf() = default;
};

class cached_context_intf
{
 public:
    virtual ~cached_context_intf() = default;

    virtual inner_service_core&
    get_service()
        = 0;
};

template<typename Value>
class uncached_request_intf
{
 public:
    virtual ~uncached_request_intf() = default;

    virtual cppcoro::task<Value>
    resolve(uncached_context_intf& ctx) const = 0;
};

template<typename Value>
class cached_request_intf
{
 public:
    virtual ~cached_request_intf() = default;

    virtual captured_id const&
    get_captured_id() const = 0;

    virtual cppcoro::task<Value>
    resolve(cached_context_intf& ctx) const = 0;
};

} // namespace cradle

#endif
