#include <atomic>

#include <cereal/types/map.hpp>
#include <spdlog/spdlog.h>

#include "thinknode_seri_v2.h"
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>
#include <cradle/thinknode/iss_req.h>
#include <cradle/thinknode/seri_catalog.h>

namespace cradle {

/**
 * Registers Thinknode v2 resolvers (which have a "proxy" counterpart).
 */

thinknode_seri_catalog_v2::thinknode_seri_catalog_v2(bool auto_register)
{
    if (auto_register)
    {
        register_all();
    }
}

void
thinknode_seri_catalog_v2::register_all()
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
        inner_.unregister_all();
        throw;
    }
    registered_ = true;
}

void
thinknode_seri_catalog_v2::try_register_all()
{
    auto sample_thinknode_info{
        make_thinknode_type_info_with_nil_type(thinknode_nil_type())};

    inner_.register_resolver(rq_post_iss_object_v2_impl(
        "sample context id", sample_thinknode_info, blob()));
}

void
thinknode_seri_catalog_v2::unregister_all()
{
    inner_.unregister_all();
    registered_ = false;
}

} // namespace cradle
