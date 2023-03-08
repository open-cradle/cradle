#ifndef CRADLE_INNER_SERVICE_REQUEST_STORE_H
#define CRADLE_INNER_SERVICE_REQUEST_STORE_H

// A service storing requests, indexed by their SHA256 hash id.
// (Storing the requests themselves, not their results.)

#include <stdexcept>

#include <fmt/format.h>

#include <cradle/inner/caching/secondary_cache_intf.h>
#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/plugins/serialization/request/cereal_json.h>

namespace cradle {

// Thrown if a request was not found in the cache
class not_found_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

// Returns a SHA256 hash for the given request.
template<FullyCachedRequest Req>
std::string
get_request_key(Req const& req)
{
    return get_unique_string(
        *req.get_captured_id(), unique_string_purpose::REQUEST);
}

// Stores a request in a secondary cache.
// A request is stored by its JSON representation, which is not space-optimal.
// On the other hand, the cache may compress the data it stores.
// May throw on errors (depending on the cache implementation).
// This is not a coroutine.
template<FullyCachedRequest Req>
cppcoro::task<void>
store_request(Req const& req, secondary_cache_intf& cache)
{
    return cache.write(
        get_request_key(req), make_blob(serialize_request(req)));
}

// Loads a request from a secondary cache.
// Throws not_found_error if the request is not in the cache.
// Throws on other errors.
// This is a coroutine. The caller should ensure that arguments passed by
// reference live long enough.
template<FullyCachedRequest Req>
cppcoro::task<Req>
load_request(std::string key, secondary_cache_intf& cache)
{
    blob req_blob{co_await cache.read(key)};
    if (!req_blob.data())
    {
        throw not_found_error(
            fmt::format("Cache has no entry with key {}", key));
    }
    co_return deserialize_request<Req>(to_string(req_blob));
}

} // namespace cradle

#endif
