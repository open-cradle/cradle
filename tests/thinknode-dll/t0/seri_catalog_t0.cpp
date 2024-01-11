#include "seri_catalog_t0.h"
#include "make_some_blob_t0_impl.h"

#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>

namespace cradle {

seri_catalog_t0::seri_catalog_t0(std::shared_ptr<seri_registry> registry)
    : selfreg_seri_catalog{std::move(registry)}
{
    register_resolver(rq_make_test_blob<caching_level_type::none>("sample"));
    register_resolver(rq_make_test_blob<caching_level_type::memory>("sample"));
    register_resolver(rq_make_test_blob<caching_level_type::full>("sample"));
}

} // namespace cradle
