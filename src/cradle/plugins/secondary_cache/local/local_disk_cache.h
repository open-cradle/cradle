#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_LOCAL_DISK_CACHE_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_LOCAL_DISK_CACHE_H

#include <memory>
#include <optional>
#include <string>

#include <BS_thread_pool.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/service/config.h>
#include <cradle/inner/service/secondary_storage_intf.h>
#include <cradle/plugins/secondary_cache/local/disk_cache_info.h>
#include <cradle/plugins/secondary_cache/local/disk_cache_poller.h>
#include <cradle/plugins/secondary_cache/local/ll_disk_cache.h>

/*
 * This file provides the top-level interface to the disk cache.
 *
 * Like the memory (immutable) cache, the disk cache implements a two-phase
 * solution, using two subcaches, called the Action Cache (AC) and the Content
 * Addressable Storage (CAS), respectively. (These terms are borrowed from the
 * Remote Execution API on https://github.com/bazelbuild/remote-apis.)
 *
 * An Action corresponds to resolving a request. Records in the Action Cache
 * are indexed by SHA-2 strings that uniquely identify a request. An AC record
 * always contains a reference to a CAS record.
 *
 * The CAS stores the result values, indexed by unique digests over those
 * values. Thus, if two different requests result in the same value, the
 * corresponding AC records will reference the same CAS record.
 * A CAS record contains a blob that serializes the actual value. Serialization
 * details are up to the disk cache client.
 */

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

    // Poll interval, in ms, for updating usage info in the database
    // (Optional integer)
    inline static std::string const POLL_INTERVAL{"disk_cache/poll_interval"};

    // (Optional boolean)
    // If true, the cache is cleared on initialization.
    inline static std::string const START_EMPTY{"disk_cache/start_empty"};

    // (Optional boolean)
    // If true, data read from a disk cache file is verified using a digest.
    inline static std::string const CHECK_FILE_DATA{
        "disk_cache/check_file_data"};
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

    bool
    allow_blob_files() const override
    {
        return true;
    }

    // Get summary information about the cache.
    disk_cache_info
    get_summary_info();

    // Reads the value stored in the database for key.
    // - Returns std::nullopt if the database has no entry for key.
    // - Returns std::nullopt if the value is stored outside the database
    //   (i.e., in a file).
    // - Returns the value (as a blob) if it is in the database.
    std::optional<blob>
    read_raw_value(std::string const& key);

    // Stores a value in the database.
    // - The value is stored in the database itself, not in a file, regardless
    //   of its size.
    void
    write_raw_value(std::string const& key, blob const& value);

    bool
    busy_writing_to_file() const;

 private:
    bool check_file_data_;
    ll_disk_cache ll_cache_;
    disk_cache_poller poller_;
    cppcoro::static_thread_pool read_pool_;
    BS::thread_pool write_pool_;
    std::shared_ptr<spdlog::logger> logger_;

    blob
    decompress_file_data(
        std::string const& key,
        ll_disk_cache_cas_entry const& entry,
        std::string const& data);
};

} // namespace cradle

#endif
