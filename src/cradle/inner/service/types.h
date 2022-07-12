#ifndef CRADLE_INNER_SERVICE_TYPES_H
#define CRADLE_INNER_SERVICE_TYPES_H

#include <optional>

#include <cradle/inner/caching/disk_cache.h>
#include <cradle/inner/caching/immutable.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/service/internals.h>

namespace cradle {

struct inner_service_config
{
    // config for the immutable memory cache
    std::optional<immutable_cache_config> immutable_cache;

    // config for the disk cache
    std::optional<disk_cache_config> disk_cache;
};

struct inner_service_core
{
    void
    inner_reset();

    void
    inner_reset(inner_service_config const& config);

    void
    inner_reset_memory_cache(immutable_cache_config const& config);

    detail::inner_service_core_internals&
    inner_internals()
    {
        return *impl_;
    }

 private:
    std::unique_ptr<detail::inner_service_core_internals> impl_;
};

} // namespace cradle

#endif
