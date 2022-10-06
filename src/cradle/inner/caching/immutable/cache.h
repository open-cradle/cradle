#ifndef CRADLE_INNER_CACHING_IMMUTABLE_CACHE_H
#define CRADLE_INNER_CACHING_IMMUTABLE_CACHE_H

#include <memory>
#include <vector>

#include <cradle/inner/core/id.h>

// This file provides the top-level interface to the immutable cache.
// This includes interfaces for instantiating a cache, configuring it, and
// inspecting its contents.

namespace cradle {

namespace detail {

struct immutable_cache_impl;

} // namespace detail

struct immutable_cache_config
{
    // The maximum amount of memory to use for caching results that are no
    // longer in use, in bytes.
    size_t unused_size_limit;
};

struct immutable_cache_info
{
    // The number of entries currently stored in the cache.
    size_t entry_count;
};

struct immutable_cache
{
    // The default constructor creates an invalid cache that must be
    // initialized via reset().
    immutable_cache();

    // Create a cache that's initialized with the given config.
    immutable_cache(immutable_cache_config config);

    ~immutable_cache();

    // Reset the cache with a new config.
    // After a successful call to this, the cache is considered initialized.
    void
    reset(immutable_cache_config config);

    // Reset the cache to an uninitialized state.
    void
    reset();

    // Is the cache initialized?
    bool
    is_initialized()
    {
        return impl ? true : false;
    }

    std::unique_ptr<detail::immutable_cache_impl> impl;
};

enum class immutable_cache_entry_state
{
    // The data isn't available yet, but it's somewhere in the process of being
    // loaded/retrieved/computed. The caller should expect that the data will
    // transition to READY without any further intervention.
    LOADING,

    // The data is available.
    READY,

    // The data failed to compute, but it could potentially be retried through
    // some external means.
    FAILED
};

struct immutable_cache_entry_snapshot
{
    // the key associated with this entry
    std::string key;

    // Is this entry ready? (i.e., Is it done being computed/retrieved?)
    immutable_cache_entry_state state;

    // type info for the cached data - valid iff data is ready
    // optional<api_type_info> type_info;

    // size of the cached data - valid iff data is ready, 0 otherwise
    size_t size;

    auto
    operator<=>(immutable_cache_entry_snapshot const& other) const
        = default;
};

struct immutable_cache_snapshot
{
    // cache entries that are currently in use
    std::vector<immutable_cache_entry_snapshot> in_use;

    // cache entries that are no longer in use and will be evicted when
    // necessary
    std::vector<immutable_cache_entry_snapshot> pending_eviction;

    // Total size of the cache entries on the eviction list.
    // The size of a cache entry in LOADING state counts as zero.
    size_t total_size_eviction_list{0};

    bool
    operator==(immutable_cache_snapshot const& other) const;

    auto
    operator<=>(immutable_cache_snapshot const& other) const;
};

// Get a snapshot of the contents of an immutable memory cache.
immutable_cache_snapshot
get_cache_snapshot(immutable_cache& cache);

// Get summary information about the cache.
immutable_cache_info
get_summary_info(immutable_cache& cache);

// Clear unused entries from the cache.
void
clear_unused_entries(immutable_cache& cache);

} // namespace cradle

#endif
