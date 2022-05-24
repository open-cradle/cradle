#ifndef CRADLE_THINKNODE_ISS_REQ_H
#define CRADLE_THINKNODE_ISS_REQ_H

#include <cppcoro/task.hpp>

#include <cradle/inner/requests/generic.h>
#include <cradle/typing/service/core.h>

namespace cradle {

class my_post_iss_object_request
{
 public:
    using value_type = std::string;

    static constexpr caching_level_type caching_level
        = caching_level_type::none;

    my_post_iss_object_request(
        // TODO also need api_url, access_token for id?
        std::string context_id,
        thinknode_type_info schema,
        blob object_data);

    cppcoro::task<value_type>
    resolve(thinknode_request_context const& ctx) const;

 private:
    std::string context_id_;
    thinknode_type_info schema_;
    blob object_data_;
};

// Create a request to post an ISS object from a raw blob of data
// (e.g. encoded in MessagePack format), and return its ID.
my_post_iss_object_request
rq_post_iss_object(
    std::string context_id, thinknode_type_info schema, blob object_data);

// Create a request to post an ISS object and return its ID.
my_post_iss_object_request
rq_post_iss_object(
    std::string context_id, thinknode_type_info schema, dynamic data);

} // namespace cradle

#endif
