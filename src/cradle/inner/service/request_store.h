#ifndef CRADLE_INNER_SERVICE_REQUEST_STORE_H
#define CRADLE_INNER_SERVICE_REQUEST_STORE_H

// A service storing requests, indexed by their SHA256 hash id.
// (Storing the requests themselves, not their results.)

#include <stdexcept>

#include <fmt/format.h>

#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/secondary_storage_intf.h>

namespace cradle {

// Thrown if a request was not found in the storage
class not_found_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

// Returns a SHA256 hash for the given request.
template<Request Req>
std::string
get_request_key(Req const& req)
{
    return get_unique_string(*req.get_captured_id());
}

// Stores a request in a secondary storage.
// A request is stored by its JSON representation, which is not space-optimal.
// On the other hand, the storage may compress the data it stores.
// May throw on errors (depending on the storage implementation).
// This is not a coroutine.
template<Request Req>
cppcoro::task<void>
store_request(Req const& req, inner_resources& resources)
{
    auto& storage{resources.secondary_cache()};
    return storage.write(
        get_request_key(req), make_blob(serialize_request(req)));
}

// Loads a request from a secondary storage.
// Throws not_found_error if the request is not in the storage.
// Throws on other errors.
// This is a coroutine. The caller should ensure that arguments passed by
// reference live long enough.
template<Request Req>
cppcoro::task<Req>
load_request(std::string key, inner_resources& resources)
{
    auto& storage{resources.secondary_cache()};
    auto opt_req_blob{co_await storage.read(key)};
    if (!opt_req_blob)
    {
        throw not_found_error(
            fmt::format("Storage has no entry with key {}", key));
    }
    co_return deserialize_request<Req>(resources, to_string(*opt_req_blob));
}

} // namespace cradle

#endif
