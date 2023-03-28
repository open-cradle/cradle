#ifndef CRADLE_INNER_SERVICE_SERI_RESOLVER_H
#define CRADLE_INNER_SERVICE_SERI_RESOLVER_H

// Objects that locally resolve a serialized request to a serialized response

#include <cassert>
#include <memory>
#include <sstream>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/request.h>
#include <cradle/inner/service/seri_result.h>
#include <cradle/plugins/serialization/request/cereal_json.h>
#include <cradle/plugins/serialization/response/msgpack.h>

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
    resolve(context_intf& ctx, std::string const& seri_req) = 0;
};

/**
 * Locally resolves a serialized request to a serialized response
 *
 * Context types at registration time and at resolution time should be
 * equal (viz. the Ctx template argument).
 * A response value must be serializable via the chosen method.
 *
 * Requests currently are always serialized via cereal-JSON.
 * Responses currently are always serialized via MessagePack.
 */
template<Context Ctx, Request Req>
class seri_resolver_impl : public seri_resolver_intf
{
 public:
    cppcoro::task<serialized_result>
    resolve(context_intf& ctx, std::string const& seri_req) override
    {
        assert(!ctx.remotely());
        Ctx* actual_ctx = dynamic_cast<Ctx*>(&ctx);
        assert(actual_ctx != nullptr);

        auto req{deserialize_request<Req>(seri_req)};
        auto value = co_await resolve_request(*actual_ctx, req);
        co_return serialized_result{serialize_response(value)};
    }
};

} // namespace cradle

#endif
