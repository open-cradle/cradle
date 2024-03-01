#include <cradle/inner/caching/immutable/internals.h>
#include <cradle/inner/caching/immutable/ptr.h>

namespace cradle {

namespace detail {

// create_task() is called with a ptr that must live until the task has run;
// the caller has to ensure this.
immutable_cache_record*
acquire_cache_record(
    immutable_cache_impl& cache,
    captured_id const& key,
    untyped_immutable_cache_ptr& ptr,
    create_task_function_t const& create_task)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    cache_record_map::iterator i = cache.records.find(&*key);
    if (i == cache.records.end())
    {
        auto record = std::make_unique<immutable_cache_record>();
        record->owner_cache = &cache;
        record->eviction_list_iterator = cache.eviction_list.end();
        record->key = key;
        record->ref_count = 0;
        record->lock_count = 0;
        record->task = create_task(ptr);
        i = cache.records.emplace(&*record->key, std::move(record)).first;
    }
    immutable_cache_record* record = i->second.get();
    // TODO: Better (optional) retry logic.
    if (record->state == immutable_cache_entry_state::FAILED)
    {
        record->task = create_task(ptr);
        record->state = immutable_cache_entry_state::LOADING;
    }
    add_ref_to_cache_record(*record);
    return record;
}

} // namespace detail

untyped_immutable_cache_ptr::untyped_immutable_cache_ptr(
    immutable_cache& cache,
    captured_id const& key,
    create_task_function_t const& create_task)
    : record_{
        *detail::acquire_cache_record(*cache.impl, key, *this, create_task)}
{
}

untyped_immutable_cache_ptr::~untyped_immutable_cache_ptr()
{
    auto& cache = *record_.owner_cache;
    std::scoped_lock<std::mutex> lock(cache.mutex);
    detail::del_ref_from_cache_record(record_);
}

void
untyped_immutable_cache_ptr::record_value_untyped(
    detail::cas_record_base::digest_type const& digest,
    detail::cas_record_maker_intf const& record_maker)
{
    auto& cache = *record_.owner_cache;
    std::scoped_lock<std::mutex> lock(cache.mutex);
    assert(record_.state == immutable_cache_entry_state::LOADING);
    record_.state = immutable_cache_entry_state::READY;
    assert(record_.cas_record == nullptr);
    auto& cas_record = cache.cas.ensure_record(digest, record_maker);
    record_.cas_record = &cas_record;
    if (record_.lock_count > 0)
    {
        cache.cas.add_lock(cas_record);
    }
}

void
untyped_immutable_cache_ptr::record_failure()
{
    auto& cache = *record_.owner_cache;
    std::scoped_lock<std::mutex> lock(cache.mutex);
    // Alternatively, make state atomic
    record_.state = immutable_cache_entry_state::FAILED;
}

} // namespace cradle
