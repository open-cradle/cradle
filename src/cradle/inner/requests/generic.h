#ifndef CRADLE_INNER_REQUESTS_GENERIC_H
#define CRADLE_INNER_REQUESTS_GENERIC_H

#include <concepts>

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

template<typename T>
concept CachedRequest
    = Request<T>&& T::caching_level != caching_level_type::none&& requires(T r)
{
    {
        r.get_captured_id()
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

template<typename T>
concept CachedContext = requires(T ctx)
{
    {
        ctx.service
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

} // namespace cradle

#endif
