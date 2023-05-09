#ifndef CRADLE_THINKNODE_ISS_H
#define CRADLE_THINKNODE_ISS_H

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/thinknode/context.h>
#include <cradle/thinknode/types.hpp>
#include <cradle/typing/service/core.h>

namespace cradle {

struct http_connection_interface;

cppcoro::task<string>
resolve_iss_object_to_immutable_uncached(
    thinknode_request_context ctx,
    string context_id,
    string object_id,
    bool ignore_upgrades);

cppcoro::task<string>
resolve_iss_object_to_immutable_generic(
    context_intf& ctx,
    string context_id,
    string object_id,
    bool ignore_upgrades);

// Resolve an ISS object to an immutable ID.
cppcoro::shared_task<string>
resolve_iss_object_to_immutable(
    thinknode_request_context ctx,
    string context_id,
    string object_id,
    bool ignore_upgrades);

cppcoro::task<std::map<string, string>>
get_iss_object_metadata_uncached(
    thinknode_request_context ctx, string context_id, string object_id);

cppcoro::task<std::map<string, string>>
get_iss_object_metadata_generic(
    context_intf& ctx, string context_id, string object_id);

// Get the metadata for an ISS object.
cppcoro::shared_task<std::map<string, string>>
get_iss_object_metadata(
    thinknode_request_context ctx, string context_id, string object_id);

// Retrieve an immutable data object.
cppcoro::shared_task<dynamic>
retrieve_immutable(
    thinknode_request_context ctx, string context_id, string immutable_id);

cppcoro::task<blob>
retrieve_immutable_blob_uncached(
    thinknode_request_context ctx, string context_id, string immutable_id);

cppcoro::task<blob>
retrieve_immutable_blob_generic(
    context_intf& ctx, string context_id, string immutable_id);

// Retrieve an immutable object as a raw blob of data (e.g. in MessagePack
// format).
cppcoro::shared_task<blob>
retrieve_immutable_blob(
    thinknode_request_context ctx, string context_id, string immutable_id);

// Get the URL form of a schema.
// The only thing needed from session is api_url, from which the default
// account name can be derived.
string
get_url_type_string(
    thinknode_session const& session, thinknode_type_info const& schema);

string
get_url_type_string(string const& api_url, thinknode_type_info const& schema);

// Converts a schema to a URL-type template string possibly containing
// {account} placeholders that need to be replaced with the real account name.
// { and } are illegal in URLs.
string
get_url_type_template(thinknode_type_info const& schema);

// Replaces any {account} placeholders in the template string with the real
// account name, derived from api_url.
string
get_url_type_string_from_template(
    string const& api_url, string const& template_string);

// Parse a schema in URL form.
thinknode_type_info
parse_url_type_string(string const& url_type);

cppcoro::task<string>
post_iss_object_uncached(
    thinknode_request_context ctx,
    string context_id,
    string url_type_string,
    blob object_data);

// A post_iss_object_uncached() variant where the schema is passed as a
// template string. Passing the schema itself (a thinknode_type_info object)
// would mean it can be serialized; an unpleasant option.
cppcoro::task<string>
post_iss_object_uncached_template_url(
    thinknode_request_context ctx,
    string context_id,
    string url_type_template_string,
    blob object_data);

cppcoro::task<string>
post_iss_object_generic_template_url(
    context_intf& ctx,
    string context_id,
    string url_type_template_string,
    blob object_data);

// Post an ISS object and return its ID.
cppcoro::shared_task<string>
post_iss_object(
    thinknode_request_context ctx,
    string context_id,
    thinknode_type_info schema,
    dynamic data);

// Post an ISS object from a raw blob of data (e.g. encoded in MessagePack
// format), and return its ID.
cppcoro::shared_task<string>
post_iss_object(
    thinknode_request_context ctx,
    string context_id,
    thinknode_type_info schema,
    blob object_data);

// Shallowly copy an ISS object from one bucket to another.
cppcoro::task<nil_t>
shallowly_copy_iss_object(
    thinknode_request_context ctx,
    string source_bucket,
    string destination_context_id,
    string object_id);

} // namespace cradle

#endif
