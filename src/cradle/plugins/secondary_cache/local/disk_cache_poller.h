#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_CACHE_POLLER_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_CACHE_POLLER_H

#include <thread>

namespace cradle {

class ll_disk_cache;

// Writes out pending AC usage to the database on polling basis.
class disk_cache_poller
{
 public:
    disk_cache_poller(ll_disk_cache& cache, int poll_interval);

 private:
    std::jthread thread_;
};

} // namespace cradle

#endif
