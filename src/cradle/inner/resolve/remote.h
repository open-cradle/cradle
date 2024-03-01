#ifndef CRADLE_INNER_RESOLVE_REMOTE_H
#define CRADLE_INNER_RESOLVE_REMOTE_H

// Service to remotely resolve requests
// No coroutines needed here

#include <string>

#include <cradle/inner/caching/immutable/lock.h>
#include <cradle/inner/remote/types.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/resolve/seri_result.h>
#include <cradle/plugins/serialization/response/msgpack.h>

namespace cradle {

// A locked record in the memory cache on a remote machine.
class remote_locked_cache_record : public locked_cache_record
{
 public:
    // The destructor accesses proxy, so the design should ensure that the
    // proxy lifetime exceeds the remote_locked_cache_record one:
    // all these objects are owned by inner_resources_impl
    // (remote_locked_cache_record indirectly through cache_record_lock),
    // and the relative data member order matches.
    remote_locked_cache_record(
        remote_proxy& proxy, remote_cache_record_id record_id);

    ~remote_locked_cache_record();

 private:
    remote_proxy& proxy_;
    remote_cache_record_id record_id_;
};

/*
 * Remotely resolves a serialized request to a serialized response
 *
 * lock_ptr, if not nullptr, refers to the memory cache lock that should be
 * set while resolving the request. The lock will refer to a memory cache
 * record on the remote.
 */
serialized_result
resolve_remote(
    remote_context_intf& ctx,
    std::string seri_req,
    cache_record_lock* lock_ptr);

/*
 * Remotely resolves a plain (non-serialized) request to a plain
 * (non-serialized) value
 *
 * lock_ptr, if not nullptr, refers to the memory cache lock that should be
 * set while resolving the request. The lock will refer to a memory cache
 * record on the remote.
 */
template<Request Req>
typename Req::value_type
resolve_remote_to_value(
    remote_context_intf& ctx, Req const& req, cache_record_lock* lock_ptr)
{
    using Value = typename Req::value_type;
    std::string seri_req{serialize_request(req)};
    auto seri_resp = resolve_remote(ctx, std::move(seri_req), lock_ptr);
    Value result = deserialize_response<Value>(seri_resp.value());
    seri_resp.on_deserialized();
    return result;
}

} // namespace cradle

#endif
