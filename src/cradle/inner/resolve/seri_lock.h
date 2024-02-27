#ifndef CRADLE_INNER_RESOLVE_SERI_LOCK_H
#define CRADLE_INNER_RESOLVE_SERI_LOCK_H

#include <cradle/inner/remote/types.h>

namespace cradle {

class cache_record_lock;

/*
 * Data related to locking a memory cache record, in the context of resolving
 * a serialized request
 *
 * If lock_ptr is not nullptr, then the memory cache record related to this
 * resolution will be locked until *lock_ptr is destroyed.
 *
 * record_id is used when resolution happens due to an RPC "resolve request"
 * call; if set, it represents a memory cache record on the server that remains
 * locked on behalf of the client, until the client releases it via a
 * release_cache_record_lock RPC call.
 */
struct seri_cache_record_lock_t
{
    cache_record_lock* lock_ptr{nullptr};
    remote_cache_record_id record_id{};
};

} // namespace cradle

#endif
