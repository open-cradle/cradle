#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_LL_DISK_CACHE_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_LL_DISK_CACHE_H

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/service/config.h>
#include <cradle/plugins/secondary_cache/local/disk_cache_info.h>

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
    std::optional<std::size_t> size_limit;
    bool start_empty{};
};

// An entry in the CAS.
struct ll_disk_cache_cas_entry
{
    // the internal numeric ID of the entry within the CAS
    int64_t cas_id;

    // The key for the entry: digest over the entry's value.
    // The digest is assumed to be unique (no collisions).
    std::string digest;

    // true iff the value is stored directly in the database
    bool in_db;

    // the value associated with the entry - This may be omitted, depending
    // on how the entry is stored in the cache and how this info was
    // queried.
    std::optional<blob> value;

    // the size of the entry, as stored in the cache (in bytes)
    int64_t size;

    // the original (decompressed) size of the entry
    int64_t original_size;
};

// This exception indicates a failure in the operation of the disk cache.
CRADLE_DEFINE_EXCEPTION(ll_disk_cache_failure)
// This provides the path to the disk cache directory.
CRADLE_DEFINE_ERROR_INFO(file_path, ll_disk_cache_path)
// This exception also provides internal_error_message_info.

struct ll_disk_cache_impl;

class ll_disk_cache
{
 public:
    // Create a disk cache that's initialized with the given config.
    // The cache starts empty (only) if config.start_empty.
    ll_disk_cache(ll_disk_cache_config const& config);

    ll_disk_cache(ll_disk_cache&& other);

    ~ll_disk_cache();

    // Reset the cache with a new config.
    // The cache is emptied (only) if config.start_empty.
    void
    reset(ll_disk_cache_config const& config);

    // Get summary information about the cache.
    disk_cache_info
    get_summary_info();

    // Get a list of all entries in the CAS.
    // None of the returned entries will include values.
    std::vector<ll_disk_cache_cas_entry>
    get_cas_entry_list();

    // Remove an individual entry from the AC; if the AC entry holds the
    // only reference to a CAS record, remove that too.
    void
    remove_entry(int64_t ac_id);

    // Clear the cache (both AC and CAS) of all data.
    void
    clear();

    // Look up an AC key in the cache.
    //
    // The returned entry is valid iff there's a valid CAS entry associated
    // with :key.
    //
    // Note that for entries stored directly in the database, this also
    // retrieves the value associated with the entry.
    //
    std::optional<ll_disk_cache_cas_entry>
    find(std::string const& ac_key);

    // Returns the ac_id for the specified AC entry if existing, or nullopt
    // otherwise
    std::optional<int64_t>
    look_up_ac_id(std::string const& ac_key);

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
        std::string const& ac_key,
        std::string const& digest,
        blob const& value,
        std::optional<std::size_t> original_size = std::nullopt);

    // Add an arbitrarily large entry to the cache.
    //
    // This is a two-part process.
    // First, you initiate the insert to get the (CAS) ID for the entry.
    // Then, once the entry is written to disk, you finish the insert.
    // (If an error occurs in between, it's OK to simply abandon the entry,
    // as it will be marked as invalid initially.)

    // Returns the cas_id for the CAS entry the caller must ultimately call
    // finish_insert() for, or nullopt if no finish_insert() is needed
    std::optional<int64_t>
    initiate_insert(std::string const& ac_key, std::string const& digest);

    // :original_size is the original size of the data;
    // it may differ from stored_size if the value is stored compressed.
    void
    finish_insert(
        int64_t cas_id, std::size_t stored_size, std::size_t original_size);

    // Given an ID within the CAS, this computes the path of the file that
    // would store the data associated with that ID (assuming that entry were
    // actually stored in a file rather than in the database).
    file_path
    get_path_for_digest(std::string const& digest);

    // Writes pending AC usage information to the database.
    // Should be called on polling basis with forced = false, where the
    // implementation decides if a write will really happen. A final call
    // before shutdown could have force = true. This could also be useful
    // for unit tests.
    void
    flush_ac_usage(bool forced = false);

 private:
    std::unique_ptr<ll_disk_cache_impl> impl_;
};

} // namespace cradle

#endif
