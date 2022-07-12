#ifndef CRADLE_INNER_SERVICE_INTERNALS_H
#define CRADLE_INNER_SERVICE_INTERNALS_H

#include <cppcoro/static_thread_pool.hpp>

#include <thread-pool/thread_pool.hpp>

#include <cradle/inner/caching/disk_cache.h>
#include <cradle/inner/caching/immutable.h>

namespace cradle {

namespace detail {

struct inner_service_core_internals
{
    cradle::immutable_cache cache;
    cradle::disk_cache disk_cache;
    cppcoro::static_thread_pool disk_read_pool;
    thread_pool disk_write_pool;

    void
    reset_memory_cache(immutable_cache_config const& config);
};

} // namespace detail

} // namespace cradle

#endif
