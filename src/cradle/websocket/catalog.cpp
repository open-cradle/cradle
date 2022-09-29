#include <regex>
#include <stdexcept>
#include <string>

#include <cereal/types/map.hpp>

#include <cradle/thinknode/iss_req.h>
#include <cradle/websocket/catalog.h>
#include <cradle/websocket/catalog_impl.h>

namespace cradle {

dynamic_resolver_registry dynamic_resolver_registry::instance_;

/**
 * Registers a dynamic resolver from a template/sample request object.
 * The resolver will be able to handle similar requests (possibly having
 * different arguments, but otherwise identical to the template).
 */
template<typename Req>
static void
register_dynamic_resolver(Req const& req)
{
    using value_type = typename Req::value_type;
    dynamic_resolver_registry::instance().register_resolver<value_type>(
        req.get_uuid().str());
}

cppcoro::task<dynamic>
resolve_serialized_request(
    thinknode_request_context& ctx, std::string const& json_text)
{
    // The uuid appears multiple times in the JSON, the first time like
    //   "polymorphic_name": "rq_retrieve_immutable_object_func+gb6df901-dirty"
    // Retrieving the uuid from the JSON text is easier than parsing the JSON.
    std::regex re("\"polymorphic_name\": \"(.+?)\"");
    std::smatch match;
    if (!std::regex_search(json_text, match, re))
    {
        throw uuid_error("no polymorphic_name found in JSON");
    }
    std::string uuid_str = match[1].str();
    std::istringstream is(json_text);
    cereal::JSONInputArchive iarchive(is);
    co_return co_await dynamic_resolver_registry::instance().resolve(
        uuid_str, ctx, iarchive);
}

/**
 * Creates a catalog of function requests that can be resolved via the
 * "resolve request" Websocket request.
 *
 * Currently limited to Thinknode requests:
 * - function_request_erased only
 * - request_props<caching_level_type::full, true, true> so
 *   - Fully cached
 *   - Function is coroutine
 *   - Introspected
 * - Resolution context is thinknode_request_context
 *
 * The first thing is that when deserializing a request descriptor received
 * via Websocket, a corresponding function_request_impl object must be created.
 * In C++, this means that corresponding constructors must exist and able to
 * be found; otherwise, cereal will complain with "Trying to load an
 * unregistered polymorphic type".
 * This is solved by registering a sample object for each type of request,
 * through the rq_..._func() calls.
 *
 * By registering the polymorphic types with cereal, it will create the
 * function_request_impl objects, but not the function_request_erased ones.
 * This will instead happen in dynamic_resolver_impl::resolve(); these _impl
 * objects must also be registered, hence the register_dynamic_resolver()
 * calls.
 */
void
create_requests_catalog()
{
    using namespace std::string_literals;
    auto sample_thinknode_info{
        make_thinknode_type_info_with_nil_type(thinknode_nil_type())};
    register_dynamic_resolver(
        rq_retrieve_immutable_object_func<caching_level_type::full>(
            "sample URL", "sample context id", "sample immutable id"s));
    register_dynamic_resolver(
        rq_post_iss_object_func<caching_level_type::full>(
            "sample URL", "sample context id", sample_thinknode_info, blob()));
    register_dynamic_resolver(
        rq_get_iss_object_metadata_func<caching_level_type::full>(
            "sample URL", "sample context id", "sample object id"s));
}

} // namespace cradle
