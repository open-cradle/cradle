#include <cradle/plugins/secondary_cache/local/disk_cache_poller.h>
#include <cradle/plugins/secondary_cache/local/ll_disk_cache.h>

#include <chrono>

namespace cradle {

static void
poll_function(std::stop_token stoken, ll_disk_cache& cache, int poll_interval)
{
    // Note: ~jthread calls request_stop() then join()
    while (!stoken.stop_requested())
    {
        cache.flush_ac_usage();
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval));
    }
}

disk_cache_poller::disk_cache_poller(ll_disk_cache& cache, int poll_interval)
    : thread_{poll_function, std::ref(cache), poll_interval}
{
}

} // namespace cradle
