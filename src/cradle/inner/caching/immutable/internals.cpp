#include <boost/functional/hash.hpp>

#include <cradle/inner/caching/immutable/internals.h>

namespace cradle {

namespace detail {

namespace {

void
add_to_eviction_list(
    cache_record_eviction_list& list, immutable_cache_record& record)
{
    assert(record.eviction_list_iterator == list.end());
    record.eviction_list_iterator = list.insert(list.end(), record);
}

void
remove_from_eviction_list(
    cache_record_eviction_list& list, immutable_cache_record& record)
{
    assert(record.eviction_list_iterator != list.end());
    list.erase(record.eviction_list_iterator);
    record.eviction_list_iterator = list.end();
}

void
reduce_memory_cache_size_impl(
    immutable_cache_impl& cache, uint64_t desired_size)
{
    // The critical size excludes CAS records with locked referrer(s).
    while (!cache.eviction_list.empty()
           && cache.cas.total_unlocked_size() > desired_size)
    {
        auto const& record = cache.eviction_list.front();
        if (auto* cas_record = record.cas_record)
        {
            cas_record->del_ref();
            if (cas_record->ref_count() == 0)
            {
                cache.cas.del_record(*cas_record);
            }
        }
        // Unlink the record, then destroy it.
        cache.eviction_list.pop_front();
        cache.records.erase(&*record.key);
    }
}

} // namespace

void
add_ref_to_cache_record(immutable_cache_record& record)
{
    auto& cache = *record.owner_cache;
    ++record.ref_count;
    if (record.eviction_list_iterator != cache.eviction_list.end())
    {
        assert(record.ref_count == 1);
        remove_from_eviction_list(cache.eviction_list, record);
    }
}

void
del_ref_from_cache_record(immutable_cache_record& record)
{
    auto& cache = *record.owner_cache;
    --record.ref_count;
    if (record.ref_count == 0)
    {
        add_to_eviction_list(cache.eviction_list, record);
        reduce_memory_cache_size_impl(cache, cache.config.unused_size_limit);
    }
}

void
add_lock_to_cache_record(immutable_cache_record& record)
{
    auto& cache = *record.owner_cache;
    ++record.lock_count;
    if (record.lock_count == 1 && record.cas_record != nullptr)
    {
        auto& cas = cache.cas;
        cas.add_lock(*record.cas_record);
    }
}

void
del_lock_from_cache_record(immutable_cache_record& record)
{
    auto& cache = *record.owner_cache;
    --record.lock_count;
    if (record.lock_count == 0 && record.cas_record != nullptr)
    {
        auto& cas = cache.cas;
        cas.del_lock(*record.cas_record);
    }
}

void
reduce_memory_cache_size(immutable_cache_impl& cache, uint64_t desired_size)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    reduce_memory_cache_size_impl(cache, desired_size);
}

std::size_t
cas_record_hash::operator()(cas_record_base::digest_type const& val) const
{
    uint8_t const* bytes = reinterpret_cast<uint8_t const*>(val.data());
    return boost::hash_range(bytes, bytes + val.size());
}

cas_record_base&
cas_cache::ensure_record(
    digest_type const& digest, cas_record_maker_intf const& record_maker)
{
    auto it = map_.find(digest);
    if (it != map_.end())
    {
        auto& existing_record = *it->second;
        existing_record.add_ref();
        return existing_record;
    }
    auto new_record = record_maker();
    auto& ret_value = *new_record;
    total_size_ += new_record->deep_size();
    [[maybe_unused]] auto [_, inserted]
        = map_.insert(std::make_pair(digest, std::move(new_record)));
    assert(inserted);
    return ret_value;
}

void
cas_cache::del_record(cas_record_base const& record)
{
    assert(record.ref_count() == 0);
    assert(record.lock_count() == 0);
    total_size_ -= record.deep_size();
    [[maybe_unused]] auto num_deleted = map_.erase(record.digest());
    assert(num_deleted == 1);
}

void
cas_cache::add_lock(cas_record_base& record)
{
    record.add_lock();
    if (record.lock_count() == 1)
    {
        total_locked_size_ += record.deep_size();
    }
}

void
cas_cache::del_lock(cas_record_base& record)
{
    record.del_lock();
    if (record.lock_count() == 0)
    {
        total_locked_size_ -= record.deep_size();
    }
}

} // namespace detail

} // namespace cradle
