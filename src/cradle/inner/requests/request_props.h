#ifndef CRADLE_INNER_REQUESTS_REQUEST_PROPS_H
#define CRADLE_INNER_REQUESTS_REQUEST_PROPS_H

#include <chrono>
#include <stdexcept>
#include <string>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

/*
 * The types of function that a request may hold, and that are used to resolve
 * that request.
 *
 * The two proxy* values are used for proxy requests, that themselves do not
 * hold any function, but represent a real request that does.
 * The distinction between proxy_plain and proxy_coro is needed by
 * normalize_arg(), if called while constructing a proxy request: the
 * subrequest's uuid must indicate whether it's for a coroutine or not.
 */
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

// A resolution retrier that prevents any retrying attempts.
class no_retrier
{
 public:
    static constexpr bool retryable = false;

    no_retrier() = default;

 protected:
    void
    save_retrier_state(JSONRequestOutputArchive& archive) const
    {
    }

    void
    load_retrier_state(JSONRequestInputArchive& archive)
    {
    }
};
static_assert(MaybeResolutionRetrier<no_retrier>);

// Retrier with an exponential backoff algorithm (base class, not instantiable)
class backoff_retrier_base
{
 public:
    int64_t
    get_base_millis() const
    {
        return base_millis_;
    }

    int
    get_max_attempts() const
    {
        return max_attempts_;
    }

 protected:
    // base_millis in milliseconds
    backoff_retrier_base(int64_t base_millis, int max_attempts);

    std::chrono::milliseconds
    attempt_retry(int attempt, std::exception const& exc) const;

    void
    save_retrier_state(JSONRequestOutputArchive& archive) const;

    // Not called for proxy_retrier
    void
    load_retrier_state(JSONRequestInputArchive& archive);

 private:
    int64_t base_millis_;
    int max_attempts_;
};

// Default retrier, implementing a hard-coded retrying algorithm.
class default_retrier : public backoff_retrier_base
{
 public:
    static constexpr bool retryable = true;

    // base_millis in milliseconds
    default_retrier(int64_t base_millis = 100, int max_attempts = 9);

    // See ResolutionRetrier
    std::chrono::milliseconds
    handle_exception(int attempt, std::exception const& exc) const;
};
static_assert(ResolutionRetrier<default_retrier>);

// Retrier suitable for a proxy, attempting to retry only when the error was
// due to RPC communication problems, not if it already was retried on the
// server
class proxy_retrier : public backoff_retrier_base
{
 public:
    static constexpr bool retryable = true;

    // base_millis in milliseconds
    proxy_retrier(int64_t base_millis = 100, int max_attempts = 9);

    // See ResolutionRetrier
    std::chrono::milliseconds
    handle_exception(int attempt, std::exception const& exc) const;
};
static_assert(ResolutionRetrier<proxy_retrier>);

/*
 * Request (resolution) properties that would be identical between similar
 * requests:
 * - Request attributes (FunctionType)
 * - How it should be resolved (Level, Introspective)
 * - A retry mechanism if any (Retrier)
 * request_props objects currently are used for classes function_request and
 * proxy_request only.
 *
 * All requests in a tree (main request, subrequests) must have a uuid, and
 * must have matching request_props types: Level, FunctionType and
 * Introspective must be identical for all requests, but Retrier may differ.
 * The properties specify the main request's uuid, which defines the complete
 * request type.
 *
 * When a request is resolved remotely, any caching happens remotely only;
 * there is no additional local caching. In particular, this means that the
 * caching level is unused for proxy requests; it must be set to "none" for
 * these requests.
 *
 * If a non-proxy request is used for a remote resolution, the request types
 * are identical between client and server. If additionally a real retrier is
 * specified, the client would retry after the server has reached the maximum
 * number of attempts; thus, the total number of attempts could be the square
 * of the retrier's max attempts parameter. Using a proxy request (with a dummy
 * retrier) therefore looks better.
 */
template<
    caching_level_type Level,
    request_function_t FunctionType = request_function_t::plain,
    bool Introspective = false,
    MaybeResolutionRetrier Retrier = no_retrier>
class request_props : public introspection_mixin<Introspective>, public Retrier
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
    static constexpr bool retryable = Retrier::retryable;
    static constexpr bool value_based_caching = is_value_based(Level);
    using retrier_type = Retrier;

    static_assert(!for_proxy || is_uncached(level));

    // Constructor for a request that does not support introspection
    explicit request_props(request_uuid uuid, Retrier retrier = Retrier())
        requires(!Introspective)
        : retrier_type{std::move(retrier)}, uuid_{std::move(uuid)}
    {
    }

    // Constructor for a request that supports introspection
    request_props(
        request_uuid uuid, std::string title, Retrier retrier = Retrier())
        requires(Introspective)
        : introspection_mixin<Introspective>{std::move(title)},
          retrier_type{std::move(retrier)},
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
