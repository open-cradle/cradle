#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_LOCAL_DISK_CACHE_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_LOCAL_DISK_CACHE_H

#include <optional>
#include <string>

#include <BS_thread_pool.hpp>
#include <cppcoro/static_thread_pool.hpp>

#include <cradle/inner/service/config.h>
#include <cradle/inner/service/secondary_storage_intf.h>
#include <cradle/plugins/secondary_cache/local/disk_cache_info.h>
#include <cradle/plugins/secondary_cache/local/ll_disk_cache.h>

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

    // (Optional boolean)
    // If true, the cache is cleared on initialization.
    inline static std::string const START_EMPTY{"disk_cache/start_empty"};
};

struct local_disk_cache_config_values
{
    // Value for the inner_config_keys::SECONDARY_CACHE_FACTORY config
    inline static std::string const PLUGIN_NAME{"local_disk_cache"};
};

class local_disk_cache : public secondary_storage_intf
{
 public:
    local_disk_cache(service_config const& config);

    void
    clear() override;

    cppcoro::task<std::optional<blob>>
    read(std::string key) override;

    cppcoro::task<void>
    write(std::string key, blob value) override;

    // Get summary information about the cache.
    disk_cache_info
    get_summary_info();

    // Reads the value stored in the database for key.
    // - Returns std::nullopt if the database has no entry for key.
    // - Returns std::nullopt if the value is stored outside the database
    //   (i.e., in a file).
    // - Returns a base64-encoded string if the value is in the database.
    std::optional<std::string>
    read_raw_value(std::string const& key);

    // Stores a value in the database.
    // - value should be base64-encoded.
    // - The value is stored in the database itself, not in a file, regardless
    //   of its size.
    void
    write_raw_value(std::string const& key, std::string const& value);

    bool
    busy_writing_to_file() const;

 private:
    ll_disk_cache ll_cache_;
    cppcoro::static_thread_pool read_pool_;
    BS::thread_pool write_pool_;
};

} // namespace cradle

#endif
