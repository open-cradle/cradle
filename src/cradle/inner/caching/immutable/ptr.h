#ifndef CRADLE_INNER_CACHING_IMMUTABLE_PTR_H
#define CRADLE_INNER_CACHING_IMMUTABLE_PTR_H

#include <cppcoro/shared_task.hpp>

#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/caching/immutable/internals.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/utilities/functional.h>

// This file provides the interface for consuming cache entries.

namespace cradle {

struct immutable_cache;

using create_task_function = function_view<std::any(
    detail::immutable_cache_impl& cache, id_interface const& key)>;

namespace detail {

struct immutable_cache_record;

// untyped_immutable_cache_ptr provides all of the functionality of
// immutable_cache_ptr without compile-time knowledge of the data type.
struct untyped_immutable_cache_ptr
{
    untyped_immutable_cache_ptr(
        immutable_cache& cache,
        captured_id const& key,
        create_task_function const& create_task);

    ~untyped_immutable_cache_ptr();

    untyped_immutable_cache_ptr(untyped_immutable_cache_ptr const& other)
        = delete;
    untyped_immutable_cache_ptr(untyped_immutable_cache_ptr&& other) = delete;
    untyped_immutable_cache_ptr&
    operator=(untyped_immutable_cache_ptr const& other)
        = delete;
    untyped_immutable_cache_ptr&
    operator=(untyped_immutable_cache_ptr&& other)
        = delete;

    id_interface const&
    key() const
    {
        return *record_.key;
    }

    immutable_cache_record*
    record() const
    {
        return &record_;
    }

 private:
    // the internal cache record for the entry
    detail::immutable_cache_record& record_;
};

} // namespace detail

void
record_immutable_cache_value(
    detail::immutable_cache_impl& cache, id_interface const& key, size_t size);

void
record_immutable_cache_failure(
    detail::immutable_cache_impl& cache, id_interface const& key);

// immutable_cache_ptr<T> represents one's interest in a particular immutable
// value (of type T). The value is assumed to be the result of performing some
// operation (with reproducible results). If there are already other parties
// interested in the result, the pointer will immediately pick up whatever
// progress has already been made in computing that result.
//
// This is a polling-based approach to observing a cache value.
//
template<class T>
struct immutable_cache_ptr
{
    immutable_cache_ptr(
        immutable_cache& cache,
        captured_id const& key,
        create_task_function const& create_task_function)
        : untyped_{cache, key, create_task_function}
    {
    }

    cppcoro::shared_task<T> const&
    task() const
    {
        return std::any_cast<cppcoro::shared_task<T> const&>(
            untyped_.record()->task);
    }

    immutable_cache_entry_state
    state() const
    {
        return untyped_.record()->state.load(std::memory_order_relaxed);
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
        return untyped_.key();
    }

 private:
    detail::untyped_immutable_cache_ptr untyped_;
};

} // namespace cradle

#endif
