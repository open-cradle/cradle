#ifndef CRADLE_INNER_CACHING_SECONDARY_CACHE_INTF_H
#define CRADLE_INNER_CACHING_SECONDARY_CACHE_INTF_H

// Interface to a secondary cache (e.g., a disk cache).
// The implementation will be provided by a plugin.

#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/service/config.h>

namespace cradle {

class secondary_cache_intf
{
 public:
    virtual ~secondary_cache_intf() = default;

    // Currently called from benchmark tests only, where it is expected to
    // empty the cache, but that's not what's happening.
    virtual void
    reset(service_config const& config)
        = 0;

    // Reads the value for key.
    // Returns blob{} if the value is not in the cache.
    virtual cppcoro::task<blob>
    read(std::string key) = 0;

    virtual cppcoro::task<void>
    write(std::string key, blob value) = 0;
};

} // namespace cradle

#endif
