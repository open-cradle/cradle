#include <cradle/inner/service/internals.h>

namespace cradle {

void
detail::inner_service_core_internals::reset_memory_cache(
    immutable_cache_config const& config)
{
    cache.reset(config);
}

void
detail::inner_service_core_internals::reset_disk_cache(
    disk_cache_config const& config)
{
    disk_cache.reset(config);
}

} // namespace cradle