#ifndef CRADLE_THINKNODE_ISS_REQ_H
#define CRADLE_THINKNODE_ISS_REQ_H

#include <tuple>

#include <cereal/types/string.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/cereal.h>
#include <cradle/thinknode/request.h>
#include <cradle/typing/encodings/msgpack.h>

namespace cradle {

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
        blob object_data);

    cppcoro::task<std::string>
    resolve(thinknode_request_context const& ctx) const;

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
        archive(api_url_, context_id_, url_type_string_, object_data_);
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
            object_data_,
            other.object_data_);
    }

 private:
    std::string api_url_;
    std::string context_id_;
    // Or a request that can calculate url_type_string_ from schema and
    // api_url? It's now always evaluated and maybe the value is not needed.
    std::string url_type_string_;
    blob object_data_;
};

template<caching_level_type level>
using my_post_iss_object_request
    = thinknode_request_container<level, my_post_iss_object_request_base>;

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
    return my_post_iss_object_request<level>{
        std::move(api_url),
        std::move(context_id),
        std::move(schema),
        std::move(object_data)};
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
    blob msgpack_data = value_to_msgpack_blob(data);
    return my_post_iss_object_request<level>{
        std::move(api_url),
        std::move(context_id),
        std::move(schema),
        std::move(msgpack_data)};
}

template<caching_level_type level>
using my_post_iss_object_request_erased = thinknode_request_erased<
    level,
    my_post_iss_object_request_base::value_type>;

template<caching_level_type level>
auto
rq_post_iss_object_erased(
    std::string api_url,
    std::string context_id,
    thinknode_type_info schema,
    blob object_data)
{
    using impl_type = thinknode_request_impl<my_post_iss_object_request_base>;
    return my_post_iss_object_request_erased<level>{
        std::make_shared<impl_type>(
            std::move(api_url),
            std::move(context_id),
            std::move(schema),
            std::move(object_data))};
}

} // namespace cradle

#endif
