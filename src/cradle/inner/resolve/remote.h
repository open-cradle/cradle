#ifndef CRADLE_INNER_RESOLVE_REMOTE_H
#define CRADLE_INNER_RESOLVE_REMOTE_H

// Service to remotely resolve requests
// No coroutines needed here

#include <string>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/resolve/seri_result.h>
#include <cradle/plugins/serialization/response/msgpack.h>

namespace cradle {

/*
 * Remotely resolves a serialized request to a serialized response
 */
serialized_result
resolve_remote(remote_context_intf& ctx, std::string seri_req);

/*
 * Remotely resolves a plain (non-serialized) request to a plain
 * (non-serialized) value
 */
template<Request Req>
typename Req::value_type
resolve_remote_to_value(remote_context_intf& ctx, Req const& req)
{
    using Value = typename Req::value_type;
    std::string seri_req{serialize_request(req)};
    auto seri_resp = resolve_remote(ctx, std::move(seri_req));
    Value result = deserialize_response<Value>(seri_resp.value());
    seri_resp.on_deserialized();
    return result;
}

} // namespace cradle

#endif
