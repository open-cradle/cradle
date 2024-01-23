#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_CACHE_INFO_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_CACHE_INFO_H

#include <string>

namespace cradle {

struct disk_cache_info
{
    // the directory containing the files for the CAS
    std::string directory;

    // maximum size of the CAS
    int64_t size_limit;

    // the number of entries currently stored in the AC
    int64_t ac_entry_count;

    // the number of entries currently stored in the CAS
    int64_t cas_entry_count;

    // the total size (in bytes) of the values in the CAS
    // (counting the stored sizes, not the original ones)
    int64_t total_size;
};

} // namespace cradle

#endif
