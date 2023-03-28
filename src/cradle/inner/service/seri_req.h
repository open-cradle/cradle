#ifndef CRADLE_INNER_SERVICE_SERI_REQ_H
#define CRADLE_INNER_SERVICE_SERI_REQ_H

// Service to resolve a serialized request to a serialized response,
// either locally or remotely

#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/seri_result.h>

namespace cradle {

/**
 * Resolves a serialized request to a serialized response
 *
 * ctx indicates where the resolution should happen: locally or remotely.
 * If the request is to be resolved locally, it must exist in the catalog
 * (otherwise, it should exist in the remote's catalog).
 *
 * Resolving a request yields a value with a request-dependent type, such as
 * int, double, blob or string.
 * Anywhere we have a serialized request, the response should also be
 * serialized. So, this function's return type is the serialized value;
 * currently(?), this will be a MessagePack string.
 */
cppcoro::task<serialized_result>
resolve_serialized_request(context_intf& ctx, std::string const& seri_req);

} // namespace cradle

#endif
