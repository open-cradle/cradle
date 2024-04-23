#ifndef CRADLE_INNER_RESOLVE_SERI_RESOLVER_H
#define CRADLE_INNER_RESOLVE_SERI_RESOLVER_H

// Objects that locally resolve a serialized request to a serialized response

#include <cassert>
#include <memory>
#include <sstream>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/encodings/msgpack_value.h>
#include <cradle/inner/requests/cast_ctx.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/resolve/seri_lock.h>
#include <cradle/inner/resolve/seri_result.h>

namespace cradle {

/**
 * Locally resolves a serialized request to a serialized response
 *
 * Abstract base class
 */
class seri_resolver_intf
{
 public:
    virtual ~seri_resolver_intf() = default;

    virtual cppcoro::task<serialized_result>
    resolve(
        local_context_intf& ctx,
        std::string seri_req,
        seri_cache_record_lock_t seri_lock)
        = 0;
};

/**
 * Locally resolves a serialized request to a serialized response
 *
 * Objects of this class will be created at registration time
 * (seri_catalog.cpp).
 *
 * A response value must be serializable via the chosen method.
 * Requests (currently?) are always serialized via cereal-JSON.
 * Responses (currently?) are always serialized via msgpack.
 */
template<Request Req>
class seri_resolver_impl : public seri_resolver_intf
{
 public:
    cppcoro::task<serialized_result>
    resolve(
        local_context_intf& ctx,
        std::string seri_req,
        seri_cache_record_lock_t seri_lock) override
    {
        assert(!ctx.remotely());
        auto req{deserialize_request<Req>(
            ctx.get_resources(), std::move(seri_req))};
        ResolutionConstraintsLocal constraints;
        auto value = co_await resolve_request(
            ctx, req, seri_lock.lock_ptr, constraints);
        co_return serialized_result{
            serialize_value(value), seri_lock.record_id};
    }
};

} // namespace cradle

#endif
