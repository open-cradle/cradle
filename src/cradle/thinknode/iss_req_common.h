#ifndef CRADLE_THINKNODE_ISS_REQ_COMMON_H
#define CRADLE_THINKNODE_ISS_REQ_COMMON_H

// Coroutines implementing the ISS functionality, wrapped on behalf of requests

#include <cereal/types/string.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/requests/generic.h>

namespace cradle {

cppcoro::task<std::string>
post_iss_object_uncached_wrapper(
    cached_introspected_context_intf& ctx_intf,
    std::string const& api_url,
    std::string const& context_id,
    std::string const& url_type_string,
    blob const& object_data);

// Differences to retrieve_immutable_blob_uncached():
// - Context is type-erased, like the request
// - Additional api_url argument passed by the framework
cppcoro::task<blob>
retrieve_immutable_blob_uncached_wrapper(
    cached_introspected_context_intf& ctx,
    std::string api_url,
    std::string context_id,
    std::string immutable_id);

// In a "resolve request" situation, ctx will outlive the resolve process,
// justifying passing it by reference.
// api_url will be discarded; is this correct?
cppcoro::task<std::map<std::string, std::string>>
get_iss_object_metadata_uncached_wrapper(
    cached_introspected_context_intf& ctx,
    std::string api_url,
    std::string context_id,
    std::string object_id);

cppcoro::task<std::string>
resolve_iss_object_to_immutable_uncached_wrapper(
    cached_introspected_context_intf& ctx,
    std::string api_url,
    std::string context_id,
    std::string object_id,
    bool ignore_upgrades);

} // namespace cradle

#endif
