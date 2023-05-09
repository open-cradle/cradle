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
    resolve(local_context_intf& ctx, std::string seri_req) = 0;
};

/**
 * Locally resolves a serialized request to a serialized response
 *
 * Objects of this class will be created at registration time
 * (seri_catalog.cpp), where Ctx=Req::ctx_type will be the context type needed
 * to resolve a request.
 * The actual type of the resolution-time context object (passed to
 * seri_resolver_intf::resolve()) should be such that a dynamic_cast to Ctx
 * succeeds; otherwise, resolve() will throw a bad_cast exception.
 * Resolution-time context objects will typically be created via the "domain"
 * class interface.
 *
 * A response value must be serializable via the chosen method.
 * Requests currently are always serialized via cereal-JSON.
 * Responses currently are always serialized via MessagePack.
 */
template<Request Req>
class seri_resolver_impl : public seri_resolver_intf
{
 public:
    cppcoro::task<serialized_result>
    resolve(local_context_intf& ctx, std::string seri_req) override
    {
        assert(!ctx.remotely());
        auto req{deserialize_request<Req>(std::move(seri_req))};
        // The intention of the "if constexpr" is to:
        // - Prevent build errors should Req not be visitable
        // - Generate less object code should Req not need async resolving
        if constexpr (ctx_in_type_list<
                          local_async_context_intf,
                          typename Req::required_ctx_types>)
        {
            if (ctx.is_async())
            {
                // Populate the context tree under ctx.
                // The request, and all subrequests, should be visitable;
                // otherwise, a compile-time error will occur.
                auto& actx = cast_ctx_to_ref<local_async_context_intf>(ctx);
                auto builder = actx.make_ctx_tree_builder();
                req.accept(*builder);
            }
        }
        ResolutionConstraintsLocal constraints;
        auto value = co_await resolve_request(ctx, req, constraints);
        co_return serialized_result{serialize_response(value)};
    }
};

} // namespace cradle

#endif
