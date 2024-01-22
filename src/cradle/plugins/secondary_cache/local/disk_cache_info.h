#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_CACHE_INFO_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_CACHE_INFO_H

#include <string>

namespace cradle {

struct disk_cache_info
{
    // the directory where the cache is stored
    std::string directory;

    // maximum size of the disk cache
    int64_t size_limit;

    // the number of entries currently stored in the cache
    int64_t entry_count;

    // the total size (in bytes)
    int64_t total_size;
};

} // namespace cradle

#endif
