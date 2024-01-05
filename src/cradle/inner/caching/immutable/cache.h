#ifndef CRADLE_INNER_CACHING_IMMUTABLE_CACHE_H
#define CRADLE_INNER_CACHING_IMMUTABLE_CACHE_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cradle/inner/core/id.h>

/*
 * This file provides the top-level interface to the immutable cache.
 * This includes interfaces for instantiating a cache, configuring it, and
 * inspecting its contents.
 *
 * The immutable cache (memory cache) implements a two-phase solution,
 * using two subcaches, called the Action Cache (AC) and the Content
 * Addressable Storage (CAS), respectively. (These terms are borrowed from the
 * Remote Execution API on https://github.com/bazelbuild/remote-apis.)
 *
 * An Action corresponds to resolving a request. Records in the Action Cache
 * are indexed by captured_id objects that uniquely identify a request.
 * An AC record contains a cppcoro::shared_task object, and an optional
 * reference to a CAS record. Running the shared task resolves the request,
 * calculating the result value, and sets the reference to the CAS record
 * (first creating a CAS record if it did not exist).
 *
 * The shared task acts as a rendez-vous for clients interested in the
 * same request, possibly at the same time. Each client performs a co_await
 * operation on the shared task, but the shared task will run only for a single
 * client; all other clients block until the calculation has finished. When
 * the co_await returns, the result is available for the client.
 *
 * The CAS stores the result values, indexed by unique digests over those
 * values. Thus, if two different requests result in the same value, the
 * corresponding AC records will reference the same CAS record.
 * A CAS record contains a copy of the value in native C++ format; there is
 * no serialization.
 *
 * For a request whose result is not yet present in the cache, the following
 * steps are performed:
 * - A new AC record is created, and a "ptr" object referencing the record
 *   is returned to the client.
 * - The client co_awaits on the shared task in the AC record.
 * - The shared task calculates the result value, and a digest over that
 *   value, then looks up or creates the CAS record.
 * - The shared task sets the CAS record reference in the AC record.
 * - A copy of the value in the CAS record is returned to the client.
 *
 * If the result value is already present, this simplifies to:
 * - A "ptr" object referencing the existing AC record is returned to the
 *   client.
 * - The client co_awaits on the shared task in the AC record, which
 *   immediately returns.
 * - The AC record contains a reference to a CAS record; a copy of the value
 *   in that CAS record is returned to the client.
 */

namespace cradle {

namespace detail {

struct immutable_cache_impl;

} // namespace detail

struct immutable_cache_config
{
    // The maximum amount of memory to use for caching results that are no
    // longer in use, in bytes.
    std::size_t unused_size_limit;
};

// Summary information on the data in the cache.
struct immutable_cache_info
{
    // Total number of AC records.
    int ac_num_records;
    // Number of in-use AC records (referenced by an immutable_cache_ptr).
    int ac_num_records_in_use;
    // Number of AC records not referenced by an immutable_cache_ptr.
    int ac_num_records_pending_eviction;
    // Total number of CAS records.
    int cas_num_records;
    // Total deep size of the values stored in the CAS.
    std::size_t cas_total_size;
};

struct immutable_cache
{
    // Create a cache that's initialized with the given config.
    explicit immutable_cache(immutable_cache_config config);

    ~immutable_cache();

    // Reset the cache with a new config, and clear its contents.
    void
    reset(immutable_cache_config config);

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

// Information on an AC record (entry).
struct immutable_cache_entry_snapshot
{
    // the key associated with this entry
    std::string key;

    // Is this entry ready? (i.e., Is it done being computed/retrieved?)
    immutable_cache_entry_state state;

    // size of the cached data - valid if data is ready, 0 otherwise
    std::size_t size;

    auto
    operator<=>(immutable_cache_entry_snapshot const& other) const
        = default;
};

std::ostream&
operator<<(std::ostream& os, immutable_cache_entry_snapshot const& entry);

// Extended information on the AC and CAS contents.
struct immutable_cache_snapshot
{
    // AC entries that are currently in use
    std::vector<immutable_cache_entry_snapshot> in_use;

    // AC entries that are no longer in use and will be evicted when necessary
    std::vector<immutable_cache_entry_snapshot> pending_eviction;

    // Total deep size of the values in the CAS.
    std::size_t total_size{0};

    bool
    operator==(immutable_cache_snapshot const& other) const;
};

std::ostream&
operator<<(std::ostream& os, immutable_cache_snapshot const& snapshot);

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
