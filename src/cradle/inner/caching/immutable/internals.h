#ifndef CRADLE_INNER_CACHING_IMMUTABLE_INTERNALS_H
#define CRADLE_INNER_CACHING_IMMUTABLE_INTERNALS_H

#include <cassert>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

#include <boost/intrusive/list.hpp>
#include <cppcoro/shared_task.hpp>

#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/core/unique_hash.h>

namespace cradle {

namespace detail {

struct immutable_cache_impl;
class cas_record_base;

/*
 * A record in the Action Cache.
 */
struct immutable_cache_record : public boost::intrusive::list_base_hook<>
{
    // These remain constant for the life of the record.
    immutable_cache_impl* owner_cache;
    captured_id key;

    // All of the following fields are protected by the cache mutex, i.e.,
    // should be accessed only while holding that mutex.

    // This is a count of how many active pointers (immutable_cache_pointer or
    // cache_record_lock) reference this data.
    // If this is 0, the data is no longer actively in use and is queued for
    // eviction. In this case, :eviction_list_iterator points to this record's
    // entry in the eviction list.
    int ref_count = 0;

    // The number of cache_record_lock objects referencing this record;
    // at most ref_count.
    int lock_count = 0;

    // (See :ref_count comment.)
    boost::intrusive::list<immutable_cache_record>::iterator
        eviction_list_iterator;

    // Is the data ready?
    immutable_cache_entry_state state = immutable_cache_entry_state::LOADING;

    // Resolves the request, stores the value in the CAS, updates this record's
    // cas_record reference; only performed for the first pointer referring to
    // the record.
    cppcoro::shared_task<void> task;

    // Reference to the CAS record, valid (non-null) after the task has run
    // (i.e., a co_await on the task has finished).
    cas_record_base* cas_record{nullptr};
};

// Indicates that a pointer started referring to the given record.
void
add_ref_to_cache_record(immutable_cache_record& record);

// Indicates that a pointer stopped referring to the given record.
void
del_ref_from_cache_record(immutable_cache_record& record);

// Adds a lock to the given record. Must be paired with an
// add_ref_to_cache_record() call.
void
add_lock_to_cache_record(immutable_cache_record& record);

// Removes a lock from the given record. Must be paired with a
// del_ref_to_cache_record() call.
void
del_lock_from_cache_record(immutable_cache_record& record);

/*
 * Unordered map storing the AC records in the AC cache.
 *
 * Based on a relatively weak hash; collisions are possible, but this hash is
 * much faster than the SHA-2 one used for the CAS. (Five times faster,
 * according to benchmarks.)
 */
typedef std::unordered_map<
    id_interface const*,
    std::unique_ptr<immutable_cache_record>,
    id_interface_pointer_hash,
    id_interface_pointer_equality_test>
    cache_record_map;

/*
 * The eviction list contains AC records in an LRU order.
 * Record ownership lies with the unordered map, not this list.
 */
using cache_record_eviction_list
    = boost::intrusive::list<immutable_cache_record>;

/*
 * Untyped base class for a record in the CAS.
 *
 * This holds a reference count of AC records referencing this CAS record,
 * and a count of how many of those AC records are locked.
 * It does not hold the (typed) value itself.
 */
class cas_record_base
{
 public:
    using digest_type = unique_hasher::result_t;

    // Creates a CAS record on behalf of an initial referring AC record.
    cas_record_base(digest_type const& digest, std::size_t deep_size)
        : digest_{digest}, deep_size_{deep_size}
    {
    }

    virtual ~cas_record_base() = default;

    digest_type const&
    digest() const
    {
        return digest_;
    }

    std::size_t
    deep_size() const
    {
        return deep_size_;
    }

    // Returns the number of AC records (locked or not) referencing this CAS
    // record.
    int
    ref_count() const
    {
        return ref_count_;
    }

    void
    add_ref()
    {
        ref_count_ += 1;
    }

    void
    del_ref()
    {
        assert(ref_count_ >= 1);
        ref_count_ -= 1;
    }

    // Returns the number of locked AC records referencing this CAS record.
    int
    lock_count() const
    {
        return lock_count_;
    }

    void
    add_lock()
    {
        lock_count_ += 1;
    }

    void
    del_lock()
    {
        assert(lock_count_ >= 1);
        lock_count_ -= 1;
    }

 private:
    digest_type digest_;
    std::size_t deep_size_;
    int ref_count_{1};
    int lock_count_{0};
};

/*
 * Calculates a hash of a CAS record digest (hash of a hash).
 */
struct cas_record_hash
{
    std::size_t
    operator()(cas_record_base::digest_type const& val) const;
};

/*
 * Typed class for a record in the CAS, storing the (typed) value.
 */
template<typename Value>
class cas_record : public cas_record_base
{
 public:
    cas_record(digest_type const& digest, Value&& value)
        : cas_record_base(digest, deep_sizeof(value))
    {
        new (&value_storage_) Value(std::move(value));
    }

    ~cas_record()
    {
        reinterpret_cast<Value*>(&value_storage_)->~Value();
    }

    Value const&
    value() const
    {
        return *reinterpret_cast<Value const*>(&value_storage_);
    }

 private:
    // Borrowed from cppcoro's shared_task_promise class
    alignas(Value) char value_storage_[sizeof(Value)];
};

/*
 * Factory of cas_record_base objects
 *
 * Type-erased class hiding the value stored in the cas_record.
 */
class cas_record_maker_intf
{
 public:
    virtual ~cas_record_maker_intf() = default;

    // Destructive / one-time operation, invalidating the factory
    virtual std::unique_ptr<cas_record_base>
    operator()() const = 0;
};

/*
 * Factory of cas_record<Value> objects
 *
 * Like cas_record itself, but storing digest and value by reference.
 */
template<typename Value>
class cas_record_maker : public cas_record_maker_intf
{
 public:
    using digest_type = cas_record_base::digest_type;

    cas_record_maker(digest_type const& digest, Value&& value)
        : digest_{digest}, value_{std::move(value)}
    {
    }

    std::unique_ptr<cas_record_base>
    operator()() const override
    {
        return std::make_unique<detail::cas_record<Value>>(
            digest_, std::move(value_));
    }

 private:
    digest_type const& digest_;
    Value&& value_;
};

/*
 * Content-addressable storage (CAS), storing the cache values, indexed by a
 * digest over the value.
 */
class cas_cache
{
 public:
    using digest_type = cas_record_base::digest_type;
    using record_ptr_type = std::unique_ptr<cas_record_base>;
    using map_type
        = std::unordered_map<digest_type, record_ptr_type, cas_record_hash>;

    // Ensure that a record exists for the given value, with the given digest.
    // If a record for the digest already exists, increases the record's
    // reference count and returns a reference to that object.
    // Otherwise, lets record_maker create a new record (with reference
    // count 1) and returns a reference to that new record.
    // If the new referrer is locked, an add_lock() follow-up call should
    // occur.
    cas_record_base&
    ensure_record(
        digest_type const& digest, cas_record_maker_intf const& record_maker);

    void
    del_record(cas_record_base const& record);

    void
    add_lock(cas_record_base& record);

    void
    del_lock(cas_record_base& record);

    int
    num_records() const
    {
        return static_cast<int>(map_.size());
    }

    // Returns the total deep size of all records in the CAS.
    std::size_t
    total_size() const
    {
        return total_size_;
    }

    // Returns the total deep size of all records in the CAS, that are referred
    // to by at least one locked AC record.
    std::size_t
    total_locked_size() const
    {
        return total_locked_size_;
    }

    // Returns the total deep size of all records in the CAS, that are referred
    // to by unlocked AC records only.
    std::size_t
    total_unlocked_size() const
    {
        return total_size_ - total_locked_size_;
    }

 private:
    map_type map_;
    std::size_t total_size_{0};
    std::size_t total_locked_size_{0};
};

struct immutable_cache_impl
{
    immutable_cache_config config;
    cache_record_map records;
    cache_record_eviction_list eviction_list;
    cas_cache cas;
    std::mutex mutex;
    int hit_count{0};
    int miss_count{0};
};

// Evict unused entries (in LRU order) until the total size of unused entries
// in the cache is at most :desired_size (in bytes).
// The cache doesn't know which entries are in use, so the criterion is instead
// based on the total size of all unlocked entries (entries that are not
// referred to by a locked AC record).
void
reduce_memory_cache_size(immutable_cache_impl& cache, uint64_t desired_size);

} // namespace detail
} // namespace cradle

#endif
