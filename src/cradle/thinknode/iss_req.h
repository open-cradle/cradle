#ifndef CRADLE_THINKNODE_ISS_REQ_H
#define CRADLE_THINKNODE_ISS_REQ_H

// ISS requests implemented using function_request_erased.
// The requests' functionality is implemented via coroutine functions
// (declared in iss_req_common.h).

#include <cereal/types/string.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/function.h>
#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/request.h>

namespace cradle {

// Properties for a Thinknode request:
// - The function is a coroutine
// - Always introspective
template<caching_level_type Level>
using thinknode_request_props
    = request_props<Level, true, true, thinknode_request_context>;

// Coroutine returning some value of type Value; in fact, Value's default value
template<typename Value>
cppcoro::task<Value>
create_default(context_intf&)
{
    co_return Value{};
}

namespace detail {

// Upgrades a raw C string to std::string, returns everything else as-is
template<typename Input>
auto
upgrade_raw_string(Input&& input)
{
    if constexpr (std::same_as<std::decay_t<Input>, const char*>)
    {
        return std::string(input);
    }
    else
    {
        return input;
    }
}

// Creates a uuid extension reflecting an argument of type Arg.
// Default case if Arg is not a request nor a blob
template<typename Arg>
std::string
make_subreq_string()
{
    return "-plain";
}

// Overload for Arg being a blob
// TODO generalize arg/subrequest uuid's
// TODO easier debugging uuid conflicts
template<typename Arg>
requires(std::same_as<Arg, blob>) std::string make_subreq_string()
{
    return "-blob";
}

// The other overloads are for Arg being a sub-request; the result depends
// on the caching level for this sub-request.
template<typename Arg>
requires(Arg::caching_level == caching_level_type::none) std::string
    make_subreq_string()
{
    return Arg::introspective ? "-subreq-none" : "-subreq-noneN";
}

template<typename Arg>
requires(Arg::caching_level == caching_level_type::memory) std::string
    make_subreq_string()
{
    return Arg::introspective ? "-subreq-mem" : "-subreq-memN";
}

template<typename Arg>
requires(Arg::caching_level == caching_level_type::full) std::string
    make_subreq_string()
{
    return Arg::introspective ? "-subreq-full" : "-subreq-fullN";
}

// Creates a request_uuid from uuid_base, extended with something reflecting
// Arg (a plain value or a sub-request)
template<typename Arg>
auto
make_ext_uuid(std::string const& uuid_base, Arg const&)
{
    return request_uuid(uuid_base + make_subreq_string<Arg>());
}

} // namespace detail

// Creates a type-erased placeholder sub-request in Thinknode context.
// To be used when registering a main request taking an input value from this
// subrequest; it is not meant to be resolved or serialized.
// Value should have a default constructor.
template<caching_level_type Level, typename Value>
auto
rq_function_thinknode_subreq()
{
    using props_type = thinknode_request_props<Level>;
    auto uuid{detail::make_ext_uuid("placeholder uuid", Value())};
    return function_request_erased<Value, props_type>(
        props_type(uuid, "placeholder title"), create_default<Value>);
}

// Creates a function_request_erased object representing a
// "post ISS object" request,
// where object_data is either a blob, or a subrequest yielding a blob.
template<caching_level_type Level, typename ObjectData>
auto
rq_post_iss_object(
    std::string context_id, thinknode_type_info schema, ObjectData object_data)
{
    auto uuid{detail::make_ext_uuid("rq_post_iss_object", object_data)};
    std::string title{"post_iss_object"};
    std::string url_type_template{get_url_type_template(schema)};
    return rq_function_erased_coro<std::string>(
        thinknode_request_props<Level>(std::move(uuid), std::move(title)),
        post_iss_object_uncached_template_url,
        std::move(context_id),
        std::move(url_type_template),
        std::move(object_data));
}

// Creates a function_request_erased object representing a
// "retrieve immutable object" request,
// where immutable_id is either a plain string, or a subrequest yielding a
// string.
//
// A uuid should uniquely identify the class type of the returned request
// object, which is a function_request_erased instantiation. So the uuid
// should depend on:
// (a) The operation (retrieve immutable object)
// (b) Caching level
// (c) immutable_id being a plain string or subrequest
// uuid_ext is an extension capturing (b) and (c).
template<caching_level_type Level, typename ImmutableId>
auto
rq_retrieve_immutable_object(std::string context_id, ImmutableId immutable_id)
{
    using value_type = blob;
    auto upgraded_immutable_id{
        detail::upgrade_raw_string(std::move(immutable_id))};
    auto uuid{detail::make_ext_uuid(
        "rq_retrieve_immutable_object", upgraded_immutable_id)};
    std::string title{"retrieve_immutable_object"};
    return rq_function_erased_coro<value_type>(
        thinknode_request_props<Level>(std::move(uuid), std::move(title)),
        retrieve_immutable_blob_uncached,
        std::move(context_id),
        std::move(upgraded_immutable_id));
}

// Creates a function_request_erased object representing a
// "get ISS object metadata" request,
// where object_id is either a plain string, or a subrequest yielding a string.
template<caching_level_type Level, typename ObjectId>
auto
rq_get_iss_object_metadata(std::string context_id, ObjectId object_id)
{
    using value_type = std::map<std::string, std::string>;
    auto upgraded_object_id{detail::upgrade_raw_string(std::move(object_id))};
    auto uuid{detail::make_ext_uuid(
        "rq_get_iss_object_metadata", upgraded_object_id)};
    std::string title{"get_iss_object_metadata"};
    return rq_function_erased_coro<value_type>(
        thinknode_request_props<Level>(std::move(uuid), std::move(title)),
        get_iss_object_metadata_uncached,
        std::move(context_id),
        std::move(upgraded_object_id));
}

// Creates a function_request_erased object representing a
// "resolve ISS object to immutable" request,
// where object_id is either a plain string, or a subrequest yielding a
// string.
// The two cases are associated with different uuid's, and the
// function_request_erased instantiations are different classes.
template<caching_level_type Level, typename ObjectId>
auto
rq_resolve_iss_object_to_immutable(
    std::string context_id, ObjectId object_id, bool ignore_upgrades)
{
    using value_type = std::string;
    auto upgraded_object_id{detail::upgrade_raw_string(std::move(object_id))};
    auto uuid{detail::make_ext_uuid(
        "rq_resolve_iss_object_to_immutable", upgraded_object_id)};
    std::string title{"resolve_iss_object_to_immutable"};
    return rq_function_erased_coro<value_type>(
        thinknode_request_props<Level>(std::move(uuid), std::move(title)),
        resolve_iss_object_to_immutable_uncached,
        std::move(context_id),
        std::move(upgraded_object_id),
        ignore_upgrades);
}

} // namespace cradle

#endif
