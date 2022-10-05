#ifndef CRADLE_THINKNODE_ISS_REQ_FUNC_H
#define CRADLE_THINKNODE_ISS_REQ_FUNC_H

// ISS requests implemented using function_request_erased.
// The requests' functionality is implemented via coroutine functions
// (declared in iss_req_common.h).

#include <cereal/types/string.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/function.h>
#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/iss_req_common.h>
#include <cradle/thinknode/request.h>

namespace cradle {

// Properties for a Thinknode request:
// - The function is a coroutine
// - Always introspected
template<caching_level_type Level>
using thinknode_request_props = request_props<Level, true, true>;

// Coroutine returning some value of type Value; in fact, Value's default value
template<typename Value>
cppcoro::task<Value>
create_default(context_intf&)
{
    co_return Value{};
}

// Creates a type-erased placeholder sub-request in Thinknode context.
// To be used when registering a main request taking an input value from this
// subrequest; it is not meant to be resolved or serialized.
// Value should have a default constructor.
template<caching_level_type Level, typename Value>
auto
rq_function_thinknode_subreq()
{
    using props_type = thinknode_request_props<Level>;
    return function_request_erased<Value, props_type>(
        props_type(request_uuid("placeholder uuid"), "placeholder title"),
        create_default<Value>);
}

namespace detail {

// uuid string extension representing the type of one argument (a subrequest,
// not a plain value), and the caching level.
template<caching_level_type Level>
struct subreq_string
{
};

template<>
struct subreq_string<caching_level_type::none>
{
    static inline std::string str{"-subreq-none"};
};

template<>
struct subreq_string<caching_level_type::memory>
{
    static inline std::string str{"-subreq-mem"};
};

template<>
struct subreq_string<caching_level_type::full>
{
    static inline std::string str{"-subreq-full"};
};

template<caching_level_type Level>
std::string
make_subreq_string()
{
    return subreq_string<Level>::str;
}

} // namespace detail

// Creates a function_request_erased object representing a
// "post ISS object" request.
template<caching_level_type Level>
auto
rq_post_iss_object_func(
    std::string api_url,
    std::string context_id,
    thinknode_type_info schema,
    blob object_data)
{
    request_uuid uuid{"rq_post_iss_object_func"};
    std::string title{"post_iss_object"};
    std::string url_type_string{get_url_type_string(api_url, schema)};
    return rq_function_erased_coro<std::string>(
        thinknode_request_props<Level>(std::move(uuid), std::move(title)),
        post_iss_object_uncached_wrapper,
        std::move(api_url),
        std::move(context_id),
        std::move(url_type_string),
        std::move(object_data));
}

namespace detail {

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
template<caching_level_type Level>
auto
rq_retrieve_immutable_object_func(
    std::string uuid_ext,
    std::string api_url,
    std::string context_id,
    auto immutable_id)
{
    std::string uuid_str{
        std::string{"rq_retrieve_immutable_object"} + uuid_ext};
    request_uuid uuid{uuid_str};
    std::string title{"retrieve_immutable_object"};
    return rq_function_erased_coro<blob>(
        thinknode_request_props<Level>(std::move(uuid), std::move(title)),
        retrieve_immutable_blob_uncached_wrapper,
        std::move(api_url),
        std::move(context_id),
        std::move(immutable_id));
}

} // namespace detail

// Creates a function_request_erased object representing a
// "retrieve immutable object" request,
// where immutable_id is a plain string.
template<caching_level_type Level>
auto
rq_retrieve_immutable_object_plain(
    std::string api_url, std::string context_id, std::string immutable_id)
{
    return detail::rq_retrieve_immutable_object_func<Level>(
        "-plain",
        std::move(api_url),
        std::move(context_id),
        std::move(immutable_id));
}

// Creates a function_request_erased object representing a
// "retrieve immutable object" request,
// where immutable_id is a subrequest yielding a string.
template<caching_level_type Level, typename ImmutableIdProps>
auto
rq_retrieve_immutable_object_subreq(
    std::string api_url,
    std::string context_id,
    function_request_erased<std::string, ImmutableIdProps> immutable_id)
{
    return detail::rq_retrieve_immutable_object_func<Level>(
        detail::make_subreq_string<ImmutableIdProps::level>(),
        std::move(api_url),
        std::move(context_id),
        std::move(immutable_id));
}

namespace detail {

// Creates a function_request_erased object representing a
// "get ISS object metadata" request,
// where object_id is either a plain string, or a subrequest yielding a string.
template<caching_level_type Level>
auto
rq_get_iss_object_metadata_func(
    std::string uuid_ext,
    std::string api_url,
    std::string context_id,
    auto object_id)
{
    using value_type = std::map<std::string, std::string>;
    std::string uuid_str{std::string{"rq_get_iss_object_metadata"} + uuid_ext};
    request_uuid uuid{uuid_str};
    std::string title{"get_iss_object_metadata"};
    return rq_function_erased_coro<value_type>(
        thinknode_request_props<Level>(std::move(uuid), std::move(title)),
        get_iss_object_metadata_uncached_wrapper,
        std::move(api_url),
        std::move(context_id),
        std::move(object_id));
}

} // namespace detail

// Creates a function_request_erased object representing a
// "get ISS object metadata" request,
// where object_id is a plain std::string.
template<caching_level_type Level>
auto
rq_get_iss_object_metadata_plain(
    std::string api_url, std::string context_id, std::string object_id)
{
    return detail::rq_get_iss_object_metadata_func<Level>(
        "-plain",
        std::move(api_url),
        std::move(context_id),
        std::move(object_id));
}

// Creates a function_request_erased object representing a
// "get ISS object metadata" request,
// where object_id is another function_request_erased object,
// with props independent from the main request.
template<caching_level_type Level, typename ObjectIdProps>
auto
rq_get_iss_object_metadata_subreq(
    std::string api_url,
    std::string context_id,
    function_request_erased<std::string, ObjectIdProps> object_id)
{
    return detail::rq_get_iss_object_metadata_func<Level>(
        detail::make_subreq_string<ObjectIdProps::level>(),
        std::move(api_url),
        std::move(context_id),
        std::move(object_id));
}

namespace detail {

// Creates a function_request_erased object representing a
// "resolve ISS object to immutable" request,
// where object_id is either a plain string, or a subrequest yielding a
// string.
// The two cases are associated with different uuid's, and the
// function_request_erased instantiations are different classes.
template<caching_level_type Level>
auto
rq_resolve_iss_object_to_immutable_func(
    std::string uuid_ext,
    std::string api_url,
    std::string context_id,
    auto object_id,
    bool ignore_upgrades)
{
    using value_type = std::string;
    std::string uuid_str{
        std::string{"rq_resolve_iss_object_to_immutable"} + uuid_ext};
    request_uuid uuid{uuid_str};
    std::string title{"resolve_iss_object_to_immutable"};
    return rq_function_erased_coro<value_type>(
        thinknode_request_props<Level>(std::move(uuid), std::move(title)),
        resolve_iss_object_to_immutable_uncached_wrapper,
        std::move(api_url),
        std::move(context_id),
        std::move(object_id),
        ignore_upgrades);
}

} // namespace detail

// Creates a function_request_erased object representing a
// "resolve ISS object to immutable" request,
// where object_id is a plain string.
template<caching_level_type Level>
auto
rq_resolve_iss_object_to_immutable_plain(
    std::string api_url,
    std::string context_id,
    std::string object_id,
    bool ignore_upgrades)
{
    return detail::rq_resolve_iss_object_to_immutable_func<Level>(
        "-plain",
        std::move(api_url),
        std::move(context_id),
        std::move(object_id),
        ignore_upgrades);
}

// Creates a function_request_erased object representing a
// "resolve ISS object to immutable" request,
// where object_id is a subrequest yielding a string.
template<caching_level_type Level, typename ObjectIdProps>
auto
rq_resolve_iss_object_to_immutable_subreq(
    std::string api_url,
    std::string context_id,
    function_request_erased<std::string, ObjectIdProps> object_id,
    bool ignore_upgrades)
{
    return detail::rq_resolve_iss_object_to_immutable_func<Level>(
        detail::make_subreq_string<ObjectIdProps::level>(),
        std::move(api_url),
        std::move(context_id),
        std::move(object_id),
        ignore_upgrades);
}

} // namespace cradle

#endif
