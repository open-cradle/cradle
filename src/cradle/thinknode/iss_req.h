#ifndef CRADLE_THINKNODE_ISS_REQ_H
#define CRADLE_THINKNODE_ISS_REQ_H

#include <cppcoro/task.hpp>

#include <cradle/inner/requests/generic.h>
#include <cradle/typing/encodings/msgpack.h>
#include <cradle/typing/service/core.h>

namespace cradle {

// my_post_iss_object_request base class not depending on caching_level_type,
// to avoid object code duplication.
class my_post_iss_object_request_base
{
 public:
    my_post_iss_object_request_base(
        // TODO also need api_url, access_token for id?
        std::string context_id,
        thinknode_type_info schema,
        blob object_data);

    captured_id const&
    get_captured_id() const
    {
        return id_;
    }

    cppcoro::task<std::string>
    resolve(thinknode_request_context const& ctx) const;

    std::string
    get_introspection_title() const
    {
        return "my_post_iss_object_request";
    }

 protected:
    ~my_post_iss_object_request_base() = default;

 private:
    std::string context_id_;
    thinknode_type_info schema_;
    blob object_data_;
    captured_id id_;

    void
    create_id();
};

template<caching_level_type level>
class my_post_iss_object_request : public my_post_iss_object_request_base
{
 public:
    using element_type = my_post_iss_object_request;
    using value_type = std::string;

    static constexpr caching_level_type caching_level = level;
    static constexpr bool introspective = true;

    using my_post_iss_object_request_base::my_post_iss_object_request_base;
};

// Create a request to post an ISS object from a raw blob of data
// (e.g. encoded in MessagePack format), and return its ID.
template<caching_level_type level>
auto
rq_post_iss_object(
    std::string context_id, thinknode_type_info schema, blob object_data)
{
    return my_post_iss_object_request<level>{
        std::move(context_id), std::move(schema), std::move(object_data)};
}

// Create a request to post an ISS object and return its ID.
template<caching_level_type level>
auto
rq_post_iss_object(
    std::string context_id, thinknode_type_info schema, dynamic data)
{
    blob msgpack_data = value_to_msgpack_blob(data);
    return my_post_iss_object_request<level>{
        std::move(context_id), std::move(schema), std::move(msgpack_data)};
}

} // namespace cradle

#endif
