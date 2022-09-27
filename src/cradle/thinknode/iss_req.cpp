#include <cassert>

#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/thinknode/utilities.h>

namespace cradle {

cppcoro::task<std::string>
post_iss_object_uncached_wrapper(
    cached_introspected_context_intf& ctx_intf,
    std::string const& api_url,
    std::string const& context_id,
    std::string const& url_type_string,
    blob const& object_data)
{
    assert(dynamic_cast<thinknode_request_context*>(&ctx_intf));
    return post_iss_object_uncached(
        static_cast<thinknode_request_context&>(ctx_intf),
        context_id,
        url_type_string,
        object_data);
}

cppcoro::task<blob>
retrieve_immutable_blob_uncached_wrapper(
    cached_introspected_context_intf& ctx,
    std::string api_url,
    std::string context_id,
    std::string immutable_id)
{
    assert(dynamic_cast<thinknode_request_context*>(&ctx));
    return retrieve_immutable_blob_uncached(
        static_cast<thinknode_request_context&>(ctx),
        std::move(context_id),
        std::move(immutable_id));
}

cppcoro::task<std::map<std::string, std::string>>
get_iss_object_metadata_uncached_wrapper(
    cached_introspected_context_intf& ctx,
    std::string api_url,
    std::string context_id,
    std::string object_id)
{
    assert(dynamic_cast<thinknode_request_context*>(&ctx));
    return get_iss_object_metadata_uncached(
        static_cast<thinknode_request_context&>(ctx),
        std::move(context_id),
        std::move(object_id));
}

} // namespace cradle
