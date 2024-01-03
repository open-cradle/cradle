#include <cradle/inner/caching/immutable/internals.h>
#include <cradle/inner/caching/immutable/ptr.h>

namespace cradle {

namespace detail {
namespace {

void
remove_from_eviction_list(
    immutable_cache_impl& cache, immutable_cache_record* record)
{
    auto& list = cache.eviction_list;
    assert(record->eviction_list_iterator != list.records.end());
    list.records.erase(record->eviction_list_iterator);
    record->eviction_list_iterator = list.records.end();
}

void
acquire_cache_record_no_lock(immutable_cache_record* record)
{
    ++record->ref_count;
    auto& evictions = record->owner_cache->eviction_list.records;
    if (record->eviction_list_iterator != evictions.end())
    {
        assert(record->ref_count == 1);
        remove_from_eviction_list(*record->owner_cache, record);
    }
}

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
        record->eviction_list_iterator = cache.eviction_list.records.end();
        record->key = key;
        record->ref_count = 0;
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
    acquire_cache_record_no_lock(record);
    return record;
}

void
add_to_eviction_list(
    immutable_cache_impl& cache, immutable_cache_record* record)
{
    auto& list = cache.eviction_list;
    assert(record->eviction_list_iterator == list.records.end());
    record->eviction_list_iterator
        = list.records.insert(list.records.end(), record);
}

void
release_cache_record(immutable_cache_record* record)
{
    auto& cache = *record->owner_cache;
    std::scoped_lock<std::mutex> lock(cache.mutex);
    --record->ref_count;
    if (record->ref_count == 0)
    {
        add_to_eviction_list(cache, record);
        reduce_memory_cache_size_no_lock(
            cache, cache.config.unused_size_limit);
    }
}

} // namespace

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
    detail::release_cache_record(&record_);
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
    record_.cas_record = &cache.cas.ensure_record(digest, record_maker);
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
