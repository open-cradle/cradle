#include <cradle/inner/caching/immutable/ptr.h>

#include <cradle/inner/caching/immutable/internals.h>

namespace cradle {

void
record_immutable_cache_value(
    detail::immutable_cache_impl& cache, id_interface const& key, size_t size)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    detail::cache_record_map::iterator i = cache.records.find(&key);
    if (i != cache.records.end())
    {
        detail::immutable_cache_record& record = *i->second;
        assert(record.state == immutable_cache_entry_state::LOADING);
        record.state = immutable_cache_entry_state::READY;
        record.size = size;
        auto& list = cache.eviction_list;
        if (record.eviction_list_iterator != list.records.end())
        {
            list.total_size += size;
        }
    }
}

void
record_immutable_cache_failure(
    detail::immutable_cache_impl& cache, id_interface const& key)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    detail::cache_record_map::iterator i = cache.records.find(&key);
    if (i != cache.records.end())
    {
        detail::immutable_cache_record& record = *i->second;
        record.state = immutable_cache_entry_state::FAILED;
    }
}

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
    if (record->state == immutable_cache_entry_state::READY)
    {
        list.total_size -= record->size;
    }
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

// create_task() is called with a key that will live until the task has run.
// Due to `key` being a shared_ptr, it can be used.
immutable_cache_record*
acquire_cache_record(
    immutable_cache_impl& cache,
    captured_id const& key,
    create_task_function const& create_task)
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
        record->task = create_task(cache, *(record->key));
        i = cache.records.emplace(&*record->key, std::move(record)).first;
    }
    immutable_cache_record* record = i->second.get();
    // TODO: Better (optional) retry logic.
    if (record->state == immutable_cache_entry_state::FAILED)
    {
        record->task = create_task(cache, *(record->key));
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
    // If record->state is LOADING, record->size will be 0, and total_size
    // should be updated on the LOADING -> READY transition.
    list.total_size += record->size;
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

// UNTYPED_IMMUTABLE_CACHE_PTR

untyped_immutable_cache_ptr::untyped_immutable_cache_ptr(
    immutable_cache& cache,
    captured_id const& key,
    create_task_function const& create_task)
    : record_{*detail::acquire_cache_record(*cache.impl, key, create_task)}
{
}

untyped_immutable_cache_ptr::~untyped_immutable_cache_ptr()
{
    detail::release_cache_record(&record_);
}

} // namespace detail

} // namespace cradle
