#include <atomic>

#include <cereal/types/map.hpp>
#include <spdlog/spdlog.h>

#include "thinknode_seri_v1.h"
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>
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
 * objects must also be registered, hence the
 * seri_catalog::register_resolver() calls.
 */

thinknode_seri_catalog_v1::thinknode_seri_catalog_v1(bool auto_register)
    : seri_catalog()
{
    if (auto_register)
    {
        register_all();
    }
}

void
thinknode_seri_catalog_v1::register_all()
{
    // TODO really need to be thread-safe?
    if (registered_.exchange(true, std::memory_order_relaxed))
    {
        auto logger{spdlog::get("cradle")};
        logger->warn("Ignoring spurious register_all() call");
        return;
    }
    try
    {
        try_register_all();
    }
    catch (...)
    {
        unregister_all();
        throw;
    }
    registered_ = true;
}

void
thinknode_seri_catalog_v1::try_register_all()
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

void
thinknode_seri_catalog_v1::unregister_all() noexcept
{
    seri_catalog::unregister_all();
    registered_ = false;
}

} // namespace cradle
