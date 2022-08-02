#ifndef CRADLE_THINKNODE_ISS_REQ_H
#define CRADLE_THINKNODE_ISS_REQ_H

#include <tuple>

#include <cereal/types/string.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/value.h>
#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/request.h>
#include <cradle/typing/encodings/msgpack.h>

namespace cradle {

cppcoro::task<std::string>
resolve_my_post_iss_object_request(
    thinknode_request_context& ctx,
    std::string const& api_url,
    std::string const& context_id,
    std::string const& url_type_string,
    blob const& object_data);

template<Request ObjectDataRequest>
requires(std::same_as<typename ObjectDataRequest::value_type, blob>)
    // The identity of a request object is formed by:
    // - The get_uuid() value, defining the class
    // - The values passed to serialize() (and to compare())
    class my_post_iss_object_request_base
{
 public:
    using value_type = std::string;

    my_post_iss_object_request_base(
        std::string api_url,
        std::string context_id,
        thinknode_type_info schema,
        ObjectDataRequest object_data_request)
        : api_url_{api_url},
          context_id_{std::move(context_id)},
          url_type_string_{get_url_type_string(api_url, schema)},
          object_data_request_{std::move(object_data_request)}
    {
    }

    cppcoro::task<std::string>
    resolve(thinknode_request_context& ctx) const
    {
        auto object_data = co_await object_data_request_.resolve(ctx);
        co_return co_await resolve_my_post_iss_object_request(
            ctx, api_url_, context_id_, url_type_string_, object_data);
    }

    // Return a string that uniquely identifies this class and its current
    // implementation. When the class's behaviour changes, then so should
    // this string.
    std::string
    get_uuid() const
    {
        return "my_post_iss_object_request";
    }

    std::string
    get_introspection_title() const
    {
        return "my_post_iss_object_request";
    }

    // Used when (de-)serializing this object (planned),
    // and for calculating its hashes.
    template<typename Archive>
    void
    serialize(Archive& archive)
    {
        archive(api_url_, context_id_, url_type_string_, object_data_request_);
    }

    // Compares against another request object, returning <0, 0, or >0.
    // The values passed to comparator are the same as in serialize();
    // it would be nice if we could somehow get rid of this duplication.
    template<typename Comparator>
    int
    compare(
        Comparator& comparator,
        my_post_iss_object_request_base const& other) const
    {
        return comparator(
            api_url_,
            other.api_url_,
            context_id_,
            other.context_id_,
            url_type_string_,
            other.url_type_string_,
            object_data_request_,
            other.object_data_request_);
    }

 private:
    std::string api_url_;
    std::string context_id_;
    // Or a request that can calculate url_type_string_ from schema and
    // api_url? It's now always evaluated and maybe the value is not needed.
    std::string url_type_string_;
    ObjectDataRequest object_data_request_;
};

template<caching_level_type level, Request ObjectDataRequest>
using my_post_iss_object_request = thinknode_request_container<
    level,
    my_post_iss_object_request_base<ObjectDataRequest>>;

// Create a request to post an ISS object, where the data are retrieved
// by resolving another request, and return the request's ID.
template<caching_level_type level, Request ObjectDataRequest>
requires(std::same_as<typename ObjectDataRequest::value_type, blob>) auto rq_post_iss_object(
    std::string api_url,
    std::string context_id,
    thinknode_type_info schema,
    ObjectDataRequest object_data_request)
{
    return my_post_iss_object_request<level, ObjectDataRequest>{
        std::move(api_url),
        std::move(context_id),
        std::move(schema),
        std::move(object_data_request)};
}

// Create a request to post an ISS object from a raw blob of data
// (e.g. encoded in MessagePack format), and return its ID.
template<caching_level_type level>
auto
rq_post_iss_object(
    std::string api_url,
    std::string context_id,
    thinknode_type_info schema,
    blob object_data)
{
    using ObjectDataRequest = value_request<blob>;
    return rq_post_iss_object<level, ObjectDataRequest>(
        std::move(api_url),
        std::move(context_id),
        std::move(schema),
        rq_value(object_data));
}

// Create a request to post an ISS object and return its ID.
template<caching_level_type level>
auto
rq_post_iss_object(
    std::string api_url,
    std::string context_id,
    thinknode_type_info schema,
    dynamic data)
{
    return rq_post_iss_object<level>(
        std::move(api_url),
        std::move(context_id),
        std::move(schema),
        rq_value(value_to_msgpack_blob(data)));
}

template<caching_level_type level, Request ObjectDataRequest>
requires(std::same_as<typename ObjectDataRequest::value_type, blob>) auto rq_post_iss_object_erased(
    std::string api_url,
    std::string context_id,
    thinknode_type_info schema,
    ObjectDataRequest object_data_request)
{
    using erased_type = thinknode_request_erased<level, std::string>;
    using impl_type = thinknode_request_impl<
        my_post_iss_object_request_base<ObjectDataRequest>>;
    return erased_type{std::make_shared<impl_type>(
        std::move(api_url),
        std::move(context_id),
        std::move(schema),
        std::move(object_data_request))};
}

template<caching_level_type level>
auto
rq_post_iss_object_erased(
    std::string api_url,
    std::string context_id,
    thinknode_type_info schema,
    blob object_data)
{
    return rq_post_iss_object_erased<level>(
        std::move(api_url),
        std::move(context_id),
        std::move(schema),
        rq_value(object_data));
}

cppcoro::task<blob>
resolve_my_retrieve_immutable_object_request(
    thinknode_request_context& ctx,
    std::string const& api_url,
    std::string const& context_id,
    std::string const& immutable_id);

template<Request ImmutableIdRequest>
requires(std::same_as<
         typename ImmutableIdRequest::value_type,
         std::string>) class my_retrieve_immutable_object_request_base
{
 public:
    using value_type = blob;

    my_retrieve_immutable_object_request_base(
        std::string api_url,
        std::string context_id,
        ImmutableIdRequest immutable_id_request)
        : api_url_{std::move(api_url)},
          context_id_{std::move(context_id)},
          immutable_id_request_{std::move(immutable_id_request)}
    {
    }

    cppcoro::task<blob>
    resolve(thinknode_request_context& ctx) const
    {
        auto immutable_id = co_await immutable_id_request_.resolve(ctx);
        co_return co_await resolve_my_retrieve_immutable_object_request(
            ctx, api_url_, context_id_, immutable_id);
    }

    std::string
    get_uuid() const
    {
        return "my_retrieve_immutable_object_request";
    }

    std::string
    get_introspection_title() const
    {
        return "my_retrieve_immutable_object_request";
    }

    template<typename Archive>
    void
    serialize(Archive& archive)
    {
        archive(api_url_, context_id_, immutable_id_request_);
    }

    template<typename Comparator>
    int
    compare(
        Comparator& comparator,
        my_retrieve_immutable_object_request_base const& other) const
    {
        return comparator(
            api_url_,
            other.api_url_,
            context_id_,
            other.context_id_,
            immutable_id_request_,
            other.immutable_id_request_);
    }

 private:
    std::string api_url_;
    std::string context_id_;
    ImmutableIdRequest immutable_id_request_;
};

template<caching_level_type level, Request ImmutableIdRequest>
requires(std::same_as<typename ImmutableIdRequest::value_type, std::string>) auto rq_retrieve_immutable_object(
    std::string api_url,
    std::string context_id,
    ImmutableIdRequest immutable_id_request)
{
    using erased_type = thinknode_request_erased<level, blob>;
    using impl_type = thinknode_request_impl<
        my_retrieve_immutable_object_request_base<ImmutableIdRequest>>;
    return erased_type{std::make_shared<impl_type>(
        std::move(api_url),
        std::move(context_id),
        std::move(immutable_id_request))};
}

template<caching_level_type level>
auto
rq_retrieve_immutable_object(
    std::string api_url, std::string context_id, std::string immutable_id)
{
    return rq_retrieve_immutable_object<level>(
        std::move(api_url), std::move(context_id), rq_value(immutable_id));
}

} // namespace cradle

#endif
