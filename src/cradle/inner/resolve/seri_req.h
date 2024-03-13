#ifndef CRADLE_INNER_RESOLVE_SERI_REQ_H
#define CRADLE_INNER_RESOLVE_SERI_REQ_H

// Service to resolve a serialized request to a serialized response,
// either locally or remotely

#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/remote/types.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/seri_lock.h>
#include <cradle/inner/resolve/seri_result.h>

namespace cradle {

/**
 * Resolves a serialized request to a serialized response
 *
 * ctx indicates where the resolution should happen: locally or remotely.
 * If the request is to be resolved locally, it must exist in the catalog
 * (otherwise, it should exist in the remote's catalog).
 * If ctx is async, this call prepares it for the upcoming resolution
 * (unlike resolve_serialized_remote() and resolve_serialized_local(), which
 * take an already prepared context).
 *
 * Resolving a request yields a value with a request-dependent type, such as
 * int, double, blob or string.
 * Anywhere we have a serialized request, the response should also be
 * serialized. So, this function's return type is the serialized value;
 * currently(?), this will be a MessagePack string.
 *
 * If resolution happens on the same machine, then seri_lock.record_id will be
 * put in the returned serialized_result value.
 * If resolution is dispatched to another machine, then seri_lock.record_id
 * will be ignored.
 */
cppcoro::task<serialized_result>
resolve_serialized_request(
    context_intf& ctx,
    std::string seri_req,
    seri_cache_record_lock_t seri_lock = seri_cache_record_lock_t{});

/**
 * Resolves a serialized request to a serialized response, remotely
 *
 * seri_lock.record_id is ignored.
 *
 * If ctx is async, it must have been prepared for the upcoming resolution.
 */
cppcoro::task<serialized_result>
resolve_serialized_remote(
    remote_context_intf& ctx,
    std::string seri_req,
    seri_cache_record_lock_t seri_lock = seri_cache_record_lock_t{});

/**
 * Resolves a serialized request to a serialized response, locally
 *
 * seri_lock.record_id will be put in the returned serialized_result value.
 *
 * If ctx is async, it must have been prepared for the upcoming resolution.
 */
cppcoro::task<serialized_result>
resolve_serialized_local(
    local_context_intf& ctx,
    std::string seri_req,
    seri_cache_record_lock_t seri_lock = seri_cache_record_lock_t{});

} // namespace cradle

#endif
