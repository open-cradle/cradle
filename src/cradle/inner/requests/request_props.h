#ifndef CRADLE_INNER_REQUESTS_REQUEST_PROPS_H
#define CRADLE_INNER_REQUESTS_REQUEST_PROPS_H

#include <string>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

// The types of function that a request may hold, and that are used to resolve
// that request.
// The two proxy* values are used for proxy requests, that themselves do not
// hold any function, but represent a real request that does.
enum class request_function_t
{
    plain, // Request holds a plain (non-coroutine) function
    coro, // Request holds a coroutine
    proxy_plain, // Request is a proxy for one that holds a plain function
    proxy_coro // Request is a proxy for one that holds a coroutine
};

// Mixin holding the introspection title, if introspection is enabled;
// otherwise, it doesn't contribute to the object and its size.
template<bool Introspective>
class introspection_mixin
{
};

template<>
class introspection_mixin<true>
{
 public:
    introspection_mixin(std::string title) : title_{std::move(title)}
    {
    }

    std::string const&
    get_title() const
    {
        return title_;
    }

 private:
    std::string title_;
};

/*
 * Request (resolution) properties that would be identical between similar
 * requests:
 * - Request attributes (FunctionType)
 * - How it should be resolved (Level, Introspective)
 *
 * All requests in a tree (main request, subrequests) must have a uuid, and
 * must have the same request_props type. The properties specify the main
 * request's uuid, which defines the complete request type.
 */
template<
    caching_level_type Level,
    request_function_t FunctionType = request_function_t::plain,
    bool Introspective = false>
class request_props : public introspection_mixin<Introspective>
{
 public:
    static constexpr caching_level_type level = Level;
    static constexpr request_function_t function_type = FunctionType;
    static constexpr bool for_plain_function
        = FunctionType == request_function_t::plain;
    static constexpr bool for_coroutine
        = FunctionType == request_function_t::coro;
    static constexpr bool for_proxy
        = FunctionType == request_function_t::proxy_plain
          || FunctionType == request_function_t::proxy_coro;
    static constexpr bool introspective = Introspective;

    // Constructor for a request that
    // - Has a uuid, so supports serialization and fully caching
    // - Does not support introspection
    explicit request_props(request_uuid uuid)
        requires(!Introspective)
        : uuid_{std::move(uuid)}
    {
    }

    // Constructor for a request that
    // - Has a uuid, so supports serialization and fully caching
    // - Supports introspection
    request_props(request_uuid uuid, std::string title)
        requires(Introspective)
        : introspection_mixin<Introspective>{std::move(title)},
          uuid_{std::move(uuid)}
    {
    }

    request_uuid const&
    get_uuid() const&
    {
        return uuid_;
    }

    request_uuid&&
    get_uuid() &&
    {
        return std::move(uuid_);
    }

 private:
    request_uuid uuid_;
};

} // namespace cradle

#endif
