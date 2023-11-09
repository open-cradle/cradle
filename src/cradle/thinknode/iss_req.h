#ifndef CRADLE_THINKNODE_ISS_REQ_H
#define CRADLE_THINKNODE_ISS_REQ_H

// ISS requests implemented using function_request and proxy_request.
// The requests' functionality is implemented via coroutine functions
// (declared in iss_req_common.h).

#include <cereal/types/string.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/function.h>
#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/request_props.h>

namespace cradle {

// Creates a function_request object representing a "post ISS object" request,
// where object_data is either a blob, or a subrequest yielding a blob.
template<caching_level_type Level, typename ObjectData>
    requires TypedArg<ObjectData, blob>
auto
rq_post_iss_object(
    std::string context_id, thinknode_type_info schema, ObjectData object_data)
{
    using props_type = thinknode_request_props<Level>;
    request_uuid uuid{"rq_post_iss_object"};
    uuid.set_level(Level);
    std::string title{"post_iss_object"};
    std::string url_type_template{get_url_type_template(schema)};
    return rq_function(
        props_type{std::move(uuid), std::move(title)},
        post_iss_object_generic_template_url,
        std::move(context_id),
        std::move(url_type_template),
        normalize_arg<blob, props_type>(std::move(object_data)));
}

// Only fully-cached requests are put in the catalog, so no need to have a
// "Level" template parameter here.
template<typename ObjectData>
    requires TypedArg<ObjectData, blob>
auto
rq_proxy_post_iss_object(
    std::string context_id, thinknode_type_info schema, ObjectData object_data)
{
    using props_type = thinknode_proxy_props;
    request_uuid uuid{"rq_post_iss_object"};
    uuid.set_level(caching_level_type::full);
    std::string title{"post_iss_object"};
    std::string url_type_template{get_url_type_template(schema)};
    return rq_proxy<std::string>(
        props_type{std::move(uuid), std::move(title)},
        std::move(context_id),
        std::move(url_type_template),
        normalize_arg<blob, props_type>(std::move(object_data)));
}

// Creates a function_request object representing a "retrieve immutable object"
// request, where immutable_id is either a plain string, or a subrequest
// yielding a string.
// TODO is perfectly forwarding immutable_id possible?
template<caching_level_type Level, typename ImmutableId>
    requires TypedArg<ImmutableId, std::string>
auto
rq_retrieve_immutable_object(std::string context_id, ImmutableId immutable_id)
{
    using props_type = thinknode_request_props<Level>;
    request_uuid uuid{"rq_retrieve_immutable_object"};
    uuid.set_level(Level);
    std::string title{"retrieve_immutable_object"};
    return rq_function(
        props_type{std::move(uuid), std::move(title)},
        retrieve_immutable_blob_generic,
        std::move(context_id),
        normalize_arg<std::string, props_type>(std::move(immutable_id)));
}

// Creates a proxy "retrieve immutable object" request.
template<typename ImmutableId>
    requires TypedArg<ImmutableId, std::string>
auto
rq_proxy_retrieve_immutable_object(
    std::string context_id, ImmutableId immutable_id)
{
    using props_type = thinknode_proxy_props;
    request_uuid uuid{"rq_retrieve_immutable_object"};
    uuid.set_level(caching_level_type::full);
    std::string title{"retrieve_immutable_object"};
    return rq_proxy<blob>(
        props_type{std::move(uuid), std::move(title)},
        std::move(context_id),
        normalize_arg<std::string, props_type>(std::move(immutable_id)));
}

// Creates a function_request object representing a "get ISS object metadata"
// request, where object_id is either a plain string, or a subrequest
// yielding a string.
template<caching_level_type Level, typename ObjectId>
    requires TypedArg<ObjectId, std::string>
auto
rq_get_iss_object_metadata(std::string context_id, ObjectId object_id)
{
    using props_type = thinknode_request_props<Level>;
    request_uuid uuid{"rq_get_iss_object_metadata"};
    uuid.set_level(Level);
    std::string title{"get_iss_object_metadata"};
    return rq_function(
        props_type(std::move(uuid), std::move(title)),
        get_iss_object_metadata_generic,
        std::move(context_id),
        normalize_arg<std::string, props_type>(std::move(object_id)));
}

// Creates a function_request object representing a "resolve ISS object to
// immutable" request, where object_id is either a plain string, or a
// subrequest yielding a string.
template<caching_level_type Level, typename ObjectId>
    requires TypedArg<ObjectId, std::string>
auto
rq_resolve_iss_object_to_immutable(
    std::string context_id, ObjectId object_id, bool ignore_upgrades)
{
    using props_type = thinknode_request_props<Level>;
    request_uuid uuid{"rq_resolve_iss_object_to_immutable"};
    uuid.set_level(Level);
    std::string title{"resolve_iss_object_to_immutable"};
    return rq_function(
        props_type(std::move(uuid), std::move(title)),
        resolve_iss_object_to_immutable_generic,
        std::move(context_id),
        normalize_arg<std::string, props_type>(std::move(object_id)),
        ignore_upgrades);
}

} // namespace cradle

#endif
