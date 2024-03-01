#include <algorithm>

#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/caching/immutable/internals.h>
#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/utilities/text.h>

namespace cradle {

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
clear_unused_entries(immutable_cache& cache)
{
    detail::reduce_memory_cache_size(*cache.impl, 0);
}

immutable_cache_info
get_summary_info(immutable_cache& cache)
{
    auto& impl = *cache.impl;
    std::scoped_lock<std::mutex> lock(impl.mutex);
    immutable_cache_info info;
    info.ac_num_records = static_cast<int>(impl.records.size());
    info.ac_num_records_pending_eviction
        = static_cast<int>(impl.eviction_list.size());
    info.ac_num_records_in_use
        = info.ac_num_records - info.ac_num_records_pending_eviction;
    info.cas_num_records = impl.cas.num_records();
    info.cas_total_size = impl.cas.total_size();
    info.cas_total_locked_size = impl.cas.total_locked_size();
    return info;
}

std::ostream&
operator<<(std::ostream& os, immutable_cache_entry_snapshot const& entry)
{
    os << "state " << static_cast<int>(entry.state) << ", size " << entry.size
       << ", key " << entry.key;
    return os;
}

std::ostream&
operator<<(std::ostream& os, immutable_cache_snapshot const& snapshot)
{
    os << snapshot.in_use.size() << " entries in use\n";
    int i = 0;
    for (auto const& entry : snapshot.in_use)
    {
        os << "[" << i << "] " << entry << "\n";
        ++i;
    }
    os << snapshot.pending_eviction.size() << " entries pending eviction\n";
    i = 0;
    for (auto const& entry : snapshot.pending_eviction)
    {
        os << "[" << i << "] " << entry << "\n";
        ++i;
    }
    return os;
}

immutable_cache_snapshot
get_cache_snapshot(immutable_cache& cache_object)
{
    auto& cache = *cache_object.impl;
    std::scoped_lock<std::mutex> lock(cache.mutex);
    immutable_cache_snapshot snapshot;
    for (auto const& [key, record] : cache.records)
    {
        immutable_cache_entry_snapshot entry{
            get_unique_string(*record->key),
            record->state,
            record->cas_record ? record->cas_record->deep_size() : 0};
        // Put the entry's info the appropriate list depending on whether
        // or not it's in the eviction list.
        if (record->eviction_list_iterator != cache.eviction_list.end())
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

// Helper struct to compare two immutable_cache_snapshot objects.
struct sorted_snapshot
{
    explicit sorted_snapshot(immutable_cache_snapshot const& unsorted);

    auto
    operator<=>(sorted_snapshot const& other) const
        = default;

 private:
    std::vector<immutable_cache_entry_snapshot> in_use;
    std::vector<immutable_cache_entry_snapshot> pending_eviction;

    void
    sort();
};

sorted_snapshot::sorted_snapshot(immutable_cache_snapshot const& unsorted)
    : in_use(unsorted.in_use), pending_eviction(unsorted.pending_eviction)
{
    sort();
}

void
sorted_snapshot::sort()
{
    auto comparator = [](immutable_cache_entry_snapshot const& a,
                         immutable_cache_entry_snapshot const& b) {
        return a.key < b.key;
    };
    std::sort(in_use.begin(), in_use.end(), comparator);
    std::sort(pending_eviction.begin(), pending_eviction.end(), comparator);
}

bool
immutable_cache_snapshot::operator==(
    immutable_cache_snapshot const& other) const
{
    return sorted_snapshot(*this) == sorted_snapshot(other);
}

} // namespace cradle
