#ifndef CRADLE_THINKNODE_ISS_REQ_H
#define CRADLE_THINKNODE_ISS_REQ_H

#include <tuple>

#include <cereal/types/string.hpp>
#include <cppcoro/task.hpp>

#include <cradle/thinknode/request.h>
#include <cradle/typing/encodings/msgpack.h>

namespace cradle {

class my_post_iss_object_request_base
{
 public:
    using value_type = std::string;
    // Tuple of arguments passed to archive()
    using tuple_type = std::tuple<std::string, std::string, std::string, blob>;

    my_post_iss_object_request_base(
        std::string api_url,
        std::string context_id,
        thinknode_type_info schema,
        blob object_data);

    cppcoro::task<std::string>
    resolve(thinknode_request_context const& ctx) const;

    std::string
    get_introspection_title() const
    {
        return "my_post_iss_object_request";
    }

    template<typename Archive>
    void
    serialize(Archive& archive)
    {
        archive(api_url_, context_id_, url_type_string_, object_data_);
    }

    template<typename Cmp>
    void
    compare(Cmp& cmp, my_post_iss_object_request_base const& other) const
    {
        cmp(api_url_,
            other.api_url_,
            context_id_,
            other.context_id_,
            url_type_string_,
            other.url_type_string_,
            object_data_,
            other.object_data_);
    }

 private:
    // id depends on these, plus something unique for this class
    // These also form the data to be (de-)serialized
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

} // namespace cradle

#endif
