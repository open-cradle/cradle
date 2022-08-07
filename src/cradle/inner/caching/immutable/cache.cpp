#include <cradle/inner/caching/immutable/cache.h>

#include <cradle/inner/caching/immutable/internals.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/utilities/text.h>

using std::string;

namespace cradle {

immutable_cache::immutable_cache() = default;

immutable_cache::~immutable_cache() = default;

immutable_cache::immutable_cache(immutable_cache_config config)
{
    this->reset(std::move(config));
}

void
immutable_cache::reset(immutable_cache_config config)
{
    this->impl = std::make_unique<detail::immutable_cache_impl>();
    this->impl->config = std::move(config);
}

void
immutable_cache::reset()
{
    this->impl.reset();
}

void
clear_unused_entries(immutable_cache& cache)
{
    detail::reduce_memory_cache_size(*cache.impl, 0);
}

immutable_cache_snapshot
get_cache_snapshot(immutable_cache& cache_object)
{
    auto& cache = *cache_object.impl;
    std::scoped_lock<std::mutex> lock(cache.mutex);
    immutable_cache_snapshot snapshot;
    snapshot.in_use.reserve(cache.records.size());
    for (auto const& [key, record] : cache.records)
    {
        immutable_cache_entry_snapshot entry{
            get_unique_string(*record->key),
            record->state,
            // is_initialized(data) ? some(data.ptr->type_info()) : none,
            record->size};
        // Put the entry's info the appropriate list depending on whether
        // or not its in the eviction list.
        if (record->eviction_list_iterator
            != cache.eviction_list.records.end())
        {
            snapshot.pending_eviction.push_back(std::move(entry));
        }
        else
        {
            snapshot.in_use.push_back(std::move(entry));
        }
    }
    return snapshot;
}

// Get summary information about the cache.
immutable_cache_info
get_summary_info(immutable_cache& cache)
{
    return detail::get_summary_info(*cache.impl);
}

} // namespace cradle
