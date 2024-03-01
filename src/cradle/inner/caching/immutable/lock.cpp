#include <stdexcept>

#include <cradle/inner/caching/immutable/lock.h>

namespace cradle {

void
cache_record_lock::set_record(std::unique_ptr<locked_cache_record> record)
{
    if (record_)
    {
        throw std::logic_error("cache_record_lock already has a lock");
    }
    record_ = std::move(record);
}

} // namespace cradle
