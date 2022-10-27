#ifndef CRADLE_INNER_SERVICE_DISK_CACHE_INTF_H
#define CRADLE_INNER_SERVICE_DISK_CACHE_INTF_H

// Disk cache interface.
// The implementation will be provided by a plugin.

#include <functional>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/service/config.h>

namespace cradle {

class disk_cache_intf
{
 public:
    virtual ~disk_cache_intf() = default;

    // Resolves a request for a blob, using some sort of disk cache.
    // These blobs will not be serialized.
    // This could be a coroutine.
    virtual cppcoro::task<blob>
    disk_cached_blob(
        captured_id key, std::function<cppcoro::task<blob>()> create_task)
        = 0;

    virtual void
    reset(service_config const& config)
        = 0;
};

} // namespace cradle

#endif
