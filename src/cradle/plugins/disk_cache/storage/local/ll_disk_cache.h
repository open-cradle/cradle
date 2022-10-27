#ifndef CRADLE_PLUGINS_DISK_CACHE_STORAGE_LOCAL_LL_DISK_CACHE_H
#define CRADLE_PLUGINS_DISK_CACHE_STORAGE_LOCAL_LL_DISK_CACHE_H

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/service/config.h>

namespace cradle {

// A disk cache is used for caching immutable data on the local hard drive to
// avoid redownloading it or recomputing it.

// The cache is implemented as a directory of files with an SQLite index
// database file that aids in tracking usage information.

// Note that a disk cache will generate exceptions any time an operation fails.
// Of course, since caching is by definition not essential to the correct
// operation of a program, there should always be a way to recover from these
// exceptions.

// A cache is internally protected by a mutex, so it can be used concurrently
// from multiple threads.

// ll_disk_cache stands for "low level disk cache": it is a helper in the
// implementation of the local disk cache.

struct ll_disk_cache_config
{
    std::optional<std::string> directory;
    std::optional<size_t> size_limit;
};

struct ll_disk_cache_info
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

struct ll_disk_cache_entry
{
    // the key for the entry
    std::string key;

    // the internal numeric ID of the entry within the cache
    int64_t id;

    // true iff the entry is stored directly in the database
    bool in_db;

    // the value associated with the entry - This may be omitted, depending
    // on how the entry is stored in the cache and how this info was
    // queried.
    std::optional<std::string> value;

    // the size of the entry, as stored in the cache (in bytes)
    int64_t size;

    // the original (decompressed) size of the entry
    int64_t original_size;

    // a 32-bit CRC of the contents of the entry
    uint32_t crc32;
};

// This exception indicates a failure in the operation of the disk cache.
CRADLE_DEFINE_EXCEPTION(ll_disk_cache_failure)
// This provides the path to the disk cache directory.
CRADLE_DEFINE_ERROR_INFO(file_path, ll_disk_cache_path)
// This exception also provides internal_error_message_info.

struct ll_disk_cache_impl;

struct ll_disk_cache
{
    // The default constructor creates an invalid disk cache that must be
    // initialized via reset().
    ll_disk_cache();

    // Create a disk cache that's initialized with the given config.
    ll_disk_cache(ll_disk_cache_config const& config);

    ~ll_disk_cache();

    // Reset the cache with a new config.
    // After a successful call to this, the cache is considered initialized.
    void
    reset(ll_disk_cache_config const& config);

    // Reset the cache to an uninitialized state.
    void
    reset();

    // Is the cache initialized?
    bool
    is_initialized()
    {
        return impl_ ? true : false;
    }

    // The rest of this interface should only be used if is_initialized()
    // returns true.

    // Get summary information about the cache.
    ll_disk_cache_info
    get_summary_info();

    // Get a list of all entries in the cache.
    // Note that none of the returned entries will include values.
    std::vector<ll_disk_cache_entry>
    get_entry_list();

    // Remove an individual entry from the cache.
    void
    remove_entry(int64_t id);

    // Clear the cache of all data.
    void
    clear();

    // Look up a key in the cache.
    //
    // The returned entry is valid iff there's a valid entry associated with
    // :key.
    //
    // Note that for entries stored directly in the database, this also
    // retrieves the value associated with the entry.
    //
    std::optional<ll_disk_cache_entry>
    find(std::string const& key);

    // Add a small entry to the cache.
    //
    // This should only be used on entries that are known to be smaller than
    // a few kB. Below this level, it is more efficient (both in time and
    // storage) to store data directly in the SQLite database.
    //
    // :original_size is the original size of the data (if it's compressed).
    // This can be omitted and the data will be understood to be uncompressed.
    //
    void
    insert(
        std::string const& key,
        std::string const& value,
        std::optional<size_t> original_size = none);

    // Add an arbitrarily large entry to the cache.
    //
    // This is a two-part process.
    // First, you initiate the insert to get the ID for the entry.
    // Then, once the entry is written to disk, you finish the insert.
    // (If an error occurs in between, it's OK to simply abandon the entry,
    // as it will be marked as invalid initially.)
    //
    int64_t
    initiate_insert(std::string const& key);
    // :original_size is the original size of the data (if it's compressed).
    // This can be omitted and the data will be understood to be uncompressed.
    void
    finish_insert(
        int64_t id,
        uint32_t crc32,
        std::optional<size_t> original_size = none);

    // Given an ID within the cache, this computes the path of the file that
    // would store the data associated with that ID (assuming that entry were
    // actually stored in a file rather than in the database).
    file_path
    get_path_for_id(int64_t id);

    // Record that an ID within the cache was just used.
    // When a lot of small objects are being read from the cache, the calls to
    // record_usage() can slow down the loading process.
    // To address this, calls are buffered and sent all at once when the cache
    // is idle.
    void
    record_usage(int64_t id);

    // If you know that the cache is idle, you can call this to force the cache
    // to write out its buffered usage records. (This is automatically called
    // when the cache is destructed.)
    void
    write_usage_records();

    // Another approach is to call this function periodically.
    // It checks to see how long it's been since the cache was last used, and
    // if the cache appears idle, it automatically writes the usage records.
    void
    do_idle_processing();

 private:
    std::unique_ptr<ll_disk_cache_impl> impl_;
};

} // namespace cradle

#endif
