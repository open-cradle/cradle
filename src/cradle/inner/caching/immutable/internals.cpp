#include <boost/functional/hash.hpp>

#include <cradle/inner/caching/immutable/internals.h>

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
    while (!cache.eviction_list.empty()
           && cache.cas.total_size() > desired_size)
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
    total_size_ -= record.deep_size();
    [[maybe_unused]] auto num_deleted = map_.erase(record.digest());
    assert(num_deleted == 1);
}

} // namespace detail

} // namespace cradle
