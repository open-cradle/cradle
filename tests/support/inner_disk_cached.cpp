#include <cradle/inner/service/disk_cached.h>
#include <cradle/inner/service/disk_cached_blob.h>

// This file needed only for test runners that don't link the outer lib,
// which already defines this disk_cached() instantiation.

namespace cradle {

template<>
cppcoro::task<blob>
disk_cached(
    inner_resources& core,
    captured_id key,
    std::function<cppcoro::task<blob>()> create_task)
{
    return disk_cached_blob(core, std::move(key), std::move(create_task));
}

} // namespace cradle
