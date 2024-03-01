#include <mutex>

#include <cradle/inner/caching/immutable/internals.h>
#include <cradle/inner/caching/immutable/local_locked_record.h>
#include <cradle/inner/caching/immutable/lock.h>

namespace cradle {

local_locked_cache_record::local_locked_cache_record(
    detail::immutable_cache_record& record)
    : record_{record}
{
    auto& cache = *record_.owner_cache;
    std::scoped_lock<std::mutex> lock(cache.mutex);
    detail::add_ref_to_cache_record(record_);
    detail::add_lock_to_cache_record(record_);
}

local_locked_cache_record::~local_locked_cache_record()
{
    auto& cache = *record_.owner_cache;
    std::scoped_lock<std::mutex> lock(cache.mutex);
    detail::del_lock_from_cache_record(record_);
    detail::del_ref_from_cache_record(record_);
}

} // namespace cradle
