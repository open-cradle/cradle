#ifndef CRADLE_INNER_CACHING_IMMUTABLE_PTR_H
#define CRADLE_INNER_CACHING_IMMUTABLE_PTR_H

#include <cppcoro/shared_task.hpp>

#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/caching/immutable/internals.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/utilities/functional.h>

// This file provides the interface for consuming cache entries.

namespace cradle {

struct immutable_cache;

namespace detail {

struct immutable_cache_record;

} // namespace detail

using ensure_value_task_t = cppcoro::shared_task<void>;
class untyped_immutable_cache_ptr;
using create_task_function_t
    = function_view<ensure_value_task_t(untyped_immutable_cache_ptr& ptr)>;

/*
 * Reference to a record in the action cache.
 *
 * The record is kept off the eviction list while at least one reference to it
 * exists. When the last reference goes away, the record is moved to the back
 * of the eviction list (LRU behavior).
 *
 * untyped_immutable_cache_ptr provides all of the functionality of
 * immutable_cache_ptr without compile-time knowledge of the data type.
 */
class untyped_immutable_cache_ptr
{
 public:
    untyped_immutable_cache_ptr(
        immutable_cache& cache,
        captured_id const& key,
        create_task_function_t const& create_task);

    virtual ~untyped_immutable_cache_ptr();

    untyped_immutable_cache_ptr(untyped_immutable_cache_ptr const& other)
        = delete;
    untyped_immutable_cache_ptr(untyped_immutable_cache_ptr&& other) = delete;
    untyped_immutable_cache_ptr&
    operator=(untyped_immutable_cache_ptr const& other)
        = delete;
    untyped_immutable_cache_ptr&
    operator=(untyped_immutable_cache_ptr&& other)
        = delete;

    // Get the record that this pointer refers to.
    // Intended to be opaque outside this module.
    detail::immutable_cache_record&
    get_record()
    {
        return record_;
    }

    // Should be called while holding the cache's mutex.
    // Used by test code only (also the three is_* functions).
    immutable_cache_entry_state
    state() const
    {
        return record_.state;
    }
    bool
    is_loading() const
    {
        return state() == immutable_cache_entry_state::LOADING;
    }
    bool
    is_ready() const
    {
        return state() == immutable_cache_entry_state::READY;
    }
    bool
    is_failed() const
    {
        return state() == immutable_cache_entry_state::FAILED;
    }

    id_interface const&
    key() const
    {
        return *record_.key;
    }

    cppcoro::shared_task<void> const&
    ensure_value_task() const
    {
        return record_.task;
    }

    void
    record_failure();

 protected:
    // the internal cache record for the entry
    detail::immutable_cache_record& record_;

    void
    record_value_untyped(
        detail::cas_record_base::digest_type const& digest,
        detail::cas_record_maker_intf const& record_maker);
};

// immutable_cache_ptr<T> represents one's interest in a particular immutable
// value (of type T). The value is assumed to be the result of performing some
// operation (with reproducible results). If there are already other parties
// interested in the result, the pointer will immediately pick up whatever
// progress has already been made in computing that result.
//
// This is a polling-based approach to observing a cache value.
//
template<typename Value>
class immutable_cache_ptr : public untyped_immutable_cache_ptr
{
 public:
    using untyped_immutable_cache_ptr::untyped_immutable_cache_ptr;

    void
    record_value(Value&& value)
    {
        unique_hasher hasher;
        update_unique_hash(hasher, value);
        auto digest{hasher.get_result()};
        record_value_untyped(
            digest, detail::cas_record_maker(digest, std::move(value)));
    }

    Value
    get_value() const
    {
        assert(record_.cas_record != nullptr);
        using typed_cas_record = detail::cas_record<Value>;
        return static_cast<typed_cas_record const*>(record_.cas_record)
            ->value();
    }
};

} // namespace cradle

#endif
