#include <cradle/inner/caching/immutable/internals.h>

#include <mutex>

#include <cradle/inner/utilities/text.h>

namespace cradle {

namespace detail {

void
reduce_memory_cache_size(immutable_cache_impl& cache, uint64_t desired_size)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    reduce_memory_cache_size_no_lock(cache, desired_size);
}

void
reduce_memory_cache_size_no_lock(
    immutable_cache_impl& cache, uint64_t desired_size)
{
    while (!cache.eviction_list.records.empty()
           && cache.eviction_list.total_size > desired_size)
    {
        auto const& record = cache.eviction_list.records.front();
        auto data_size = record->size;
        cache.records.erase(&*record->key);
        cache.eviction_list.records.pop_front();
        cache.eviction_list.total_size -= data_size;
    }
}

immutable_cache_info
get_summary_info(immutable_cache_impl& cache)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    immutable_cache_info info;
    info.entry_count = cache.records.size();
    return info;
}

} // namespace detail

} // namespace cradle
