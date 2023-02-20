#include <cereal/types/map.hpp>

#include <cradle/inner/service/seri_catalog.h>
#include <cradle/plugins/serialization/disk_cache/preferred/cereal/cereal.h>
#include <cradle/thinknode/iss_req.h>

namespace cradle {

/**
 * Registers resolvers that can resolve serialized Thinknode requests
 *
 * (Current?) limitations:
 * - function_request_erased only
 * - request_props<caching_level_type::full, true, true> so
 *   - Fully cached
 *   - Function is coroutine
 *   - Introspective
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
 * function_request_impl objects, but not the function_request_erased ones.
 * This will instead happen in seri_resolver_impl::resolve(); these _impl
 * objects must also be registered, hence the register_seri_resolver()
 * calls.
 */
namespace {

template<Request Req>
void
register_tn_resolver(Req const& req)
{
    using Ctx = thinknode_request_context;
    register_seri_resolver<Ctx, Req>(req);
}

} // namespace

void
register_thinknode_seri_resolvers()
{
    constexpr caching_level_type level = caching_level_type::full;
    auto sample_thinknode_info{
        make_thinknode_type_info_with_nil_type(thinknode_nil_type())};

    // Note that all value-or-subrequest arguments are values here, so that
    // the "normalizing" subrequests also get registered.
    // A (maybe better) alternative would be to register these subrequests
    // independently.
    register_tn_resolver(rq_retrieve_immutable_object<level>(
        "sample context id", "sample immutable id"));
    register_tn_resolver(rq_post_iss_object<level>(
        "sample context id", sample_thinknode_info, blob()));
    register_tn_resolver(rq_get_iss_object_metadata<level>(
        "sample context id", "sample object id"));
    register_tn_resolver(rq_resolve_iss_object_to_immutable<level>(
        "sample context id", "sample object id", false));
}

} // namespace cradle
