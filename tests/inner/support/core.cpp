#include "../support/core.h"
#include <cradle/inner/fs/types.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/service/core.h>

namespace cradle {

void
init_test_inner_service(inner_service_core& core)
{
    auto cache_dir = file_path("tests_inner_disk_cache");

    reset_directory(cache_dir);

    core.inner_reset(inner_service_config{
        immutable_cache_config{0x40'00'00'00},
        disk_cache_config{cache_dir.string(), 0x40'00'00'00}});
}

cached_request_resolution_context::cached_request_resolution_context()
{
    init_test_inner_service(service);
}

void
cached_request_resolution_context::reset_memory_cache()
{
    service.inner_reset_memory_cache(immutable_cache_config{0x40'00'00'00});
}

} // namespace cradle
