#include <utility>

#include <cereal/types/map.hpp>

#include "thinknode_seri_v1.h"
#include <cradle/thinknode/iss_req.h>
#include <cradle/typing/core/unique_hash.h>

namespace cradle {

/**
 * Registers resolvers that can resolve serialized Thinknode requests
 *
 * These Thinknode requests are implemented by instantiations of class
 * function_request, with the following properties:
 * - Fully cached
 * - Function is coroutine
 * - Introspective
 *
 * The first thing is that when deserializing a JSON-serialized request,
 * a corresponding function_request_impl object must be created.
 * In C++, this means that corresponding constructors must exist and able to
 * be found; otherwise, cereal will complain with "Trying to load an
 * unregistered polymorphic type".
 * This is solved by registering a sample object for each type of request,
 * through the rq_...() calls.
 *
 * By registering the polymorphic types with cereal, it will create the
 * function_request_impl objects, but not the function_request ones.
 * This will instead happen in seri_resolver_impl::resolve(); these _impl
 * objects must also be registered, hence the
 * seri_catalog::register_resolver() calls.
 */
thinknode_seri_catalog_v1::thinknode_seri_catalog_v1(
    std::shared_ptr<seri_registry> registry)
    : selfreg_seri_catalog{std::move(registry)}
{
    constexpr caching_level_type level = caching_level_type::full;
    auto sample_thinknode_info{
        make_thinknode_type_info_with_nil_type(thinknode_nil_type())};

    // Note that all value-or-subrequest arguments are values here, so that
    // the "normalizing" subrequests also get registered.
    // A (maybe better) alternative would be to register these subrequests
    // independently.
    register_resolver(rq_retrieve_immutable_object<level>(
        "sample context id", "sample immutable id"));
    register_resolver(rq_post_iss_object<level>(
        "sample context id", sample_thinknode_info, blob()));
    register_resolver(rq_get_iss_object_metadata<level>(
        "sample context id", "sample object id"));
    register_resolver(rq_resolve_iss_object_to_immutable<level>(
        "sample context id", "sample object id", false));
}

} // namespace cradle
