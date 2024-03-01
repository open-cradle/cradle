#ifndef CRADLE_INNER_CACHING_IMMUTABLE_LOCAL_LOCKED_RECORD_H
#define CRADLE_INNER_CACHING_IMMUTABLE_LOCAL_LOCKED_RECORD_H

#include <cradle/inner/caching/immutable/lock.h>

namespace cradle {

namespace detail {

struct immutable_cache_record;

} // namespace detail

// A locked record in the memory cache on the local machine.
class local_locked_cache_record : public locked_cache_record
{
 public:
    local_locked_cache_record(detail::immutable_cache_record& record);

    ~local_locked_cache_record();

 private:
    detail::immutable_cache_record& record_;
};

} // namespace cradle

#endif
