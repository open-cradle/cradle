#ifndef CRADLE_INNER_REQUESTS_GENERIC_H
#define CRADLE_INNER_REQUESTS_GENERIC_H

#include <concepts>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>

namespace cradle {

struct immutable_cache;
struct inner_service_core;
class tasklet_tracker;

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

// unique_ptr<Request> or shared_ptr<Request>
template<typename T>
concept RequestPtr = Request<typename T::element_type>;

template<typename T>
concept RequestOrPtr = Request<T> || RequestPtr<T>;

template<typename T>
concept UncachedRequest
    = Request<T>&& T::caching_level == caching_level_type::none;

template<typename T>
concept UncachedRequestPtr = UncachedRequest<typename T::element_type>;

template<typename T>
concept UncachedRequestOrPtr = UncachedRequest<T> || UncachedRequestPtr<T>;

template<typename Req>
concept CachedRequest
    = Request<Req>&& Req::caching_level != caching_level_type::none&& requires
{
    std::same_as<decltype(Req::introspective), bool>;
}
&&requires(Req const& req)
{
    {
        req.get_captured_id()
    }
    ->std::convertible_to<captured_id const&>;
};

template<typename Req>
concept FullyCachedRequest
    = CachedRequest<Req>&& Req::caching_level == caching_level_type::full;

template<typename Req>
concept IntrospectiveRequest
    = CachedRequest<Req>&& Req::introspective&& requires(Req const& req)
{
    {
        req.get_introspection_title()
    }
    ->std::convertible_to<std::string>;
};

template<typename T>
concept CachedRequestPtr = CachedRequest<typename T::element_type>;

template<typename T>
concept CachedRequestOrPtr = CachedRequest<T> || CachedRequestPtr<T>;

template<typename T>
concept RequestContext = true;

template<typename T>
concept UncachedContext = true;

template<typename Ctx>
concept CachedContext = requires(Ctx& ctx)
{
    {
        ctx.get_service()
    }
    ->std::convertible_to<inner_service_core&>;
    {
        ctx.get_cache()
    }
    ->std::convertible_to<immutable_cache&>;
};

template<typename Ctx>
concept IntrospectiveContext = CachedContext<Ctx>&& requires(Ctx& ctx)
{
    {
        ctx.get_tasklet()
    }
    ->std::convertible_to<tasklet_tracker const*>;
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

template<typename Ctx, typename Req>
concept FullyCachedContextRequest = CachedContext<Ctx>&&
    FullyCachedRequest<Req>&& MatchingContextRequest<Ctx, Req>;

template<typename Ctx, typename Req>
concept IntrospectiveContextRequest = IntrospectiveContext<Ctx>&&
    IntrospectiveRequest<Req>&& MatchingContextRequest<Ctx, Req>;

class context_intf
{
 public:
    virtual ~context_intf() = default;

    // Only implemented in cached context
    // TODO create cached_context_intf class
    virtual inner_service_core&
    get_service()
        = 0;

    // Only implemented in cached context
    virtual immutable_cache&
    get_cache()
        = 0;

    // Only implemented in cached context
    virtual tasklet_tracker*
    get_tasklet()
        = 0;

    // Only implemented in cached context
    virtual void
    push_tasklet(tasklet_tracker* tasklet)
        = 0;

    // Only implemented in cached context
    virtual void
    pop_tasklet()
        = 0;
};

class context_tasklet
{
 public:
    context_tasklet(
        context_intf& ctx,
        std::string const& pool_name,
        std::string const& title);

    ~context_tasklet();

 private:
    context_intf* ctx_{nullptr};
};

} // namespace cradle

#endif
