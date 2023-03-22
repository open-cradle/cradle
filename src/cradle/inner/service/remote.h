#ifndef CRADLE_INNER_SERVICE_REMOTE_H
#define CRADLE_INNER_SERVICE_REMOTE_H

// Service to remotely resolve requests

#include <string>

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/seri_result.h>
#include <cradle/plugins/serialization/request/cereal_json.h>
#include <cradle/plugins/serialization/response/msgpack.h>

namespace cradle {

// Remotely resolves a serialized request to a serialized response
cppcoro::task<serialized_result>
resolve_remote(remote_context_intf& ctx, std::string const& seri_req);

/* Remotely resolves a plain request to a plain value
 *
 * Will be called from resolve_request*() in inner/service/request.h,
 * which return a shared_task.
 */
template<Request Req>
cppcoro::shared_task<typename Req::value_type>
resolve_remote_to_value(remote_context_intf& ctx, Req const& req)
{
    using Value = typename Req::value_type;
    std::string seri_req{serialize_request(req)};
    auto seri_resp = co_await resolve_remote(ctx, seri_req);
    Value result = deserialize_response<Value>(seri_resp.value());
    seri_resp.on_deserialized();
    co_return result;
}

} // namespace cradle

#endif
