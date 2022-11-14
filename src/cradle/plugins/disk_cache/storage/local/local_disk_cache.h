#ifndef CRADLE_PLUGINS_DISK_CACHE_STORAGE_LOCAL_LOCAL_DISK_CACHE_H
#define CRADLE_PLUGINS_DISK_CACHE_STORAGE_LOCAL_LOCAL_DISK_CACHE_H

#include <cppcoro/static_thread_pool.hpp>
#include <thread-pool/thread_pool.hpp>

#include <cradle/inner/service/config.h>
#include <cradle/inner/service/disk_cache_intf.h>
#include <cradle/plugins/disk_cache/storage/local/ll_disk_cache.h>

namespace cradle {

// Configuration keys for the local storage plugin
struct local_disk_cache_config_keys
{
    // (Optional string)
    inline static std::string const DIRECTORY{"disk_cache/directory"};

    // (Optional integer)
    inline static std::string const SIZE_LIMIT{"disk_cache/size_limit"};

    // (Optional integer)
    inline static std::string const NUM_THREADS_READ_POOL{
        "disk_cache/num_threads_read_pool"};

    // (Optional integer)
    inline static std::string const NUM_THREADS_WRITE_POOL{
        "disk_cache/num_threads_write_pool"};
};

struct local_disk_cache_config_values
{
    // Value for the inner_config_keys::DISK_CACHE_FACTORY config
    inline static std::string const PLUGIN_NAME{"local_disk_cache"};
};

class local_disk_cache : public disk_cache_intf
{
 public:
    local_disk_cache(service_config const& config);

    cppcoro::task<blob>
    disk_cached_blob(
        captured_id key,
        std::function<cppcoro::task<blob>()> create_task) override;

    void
    reset(service_config const& config) override;

    ll_disk_cache&
    get_ll_disk_cache()
    {
        return ll_cache_;
    }

    cppcoro::static_thread_pool&
    read_pool()
    {
        return read_pool_;
    }

    thread_pool&
    write_pool()
    {
        return write_pool_;
    }

 private:
    ll_disk_cache ll_cache_;
    cppcoro::static_thread_pool read_pool_;
    thread_pool write_pool_;
};

} // namespace cradle

#endif
