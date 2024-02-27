#ifndef CRADLE_INNER_CACHING_IMMUTABLE_LOCK_H
#define CRADLE_INNER_CACHING_IMMUTABLE_LOCK_H

#include <memory>

namespace cradle {

// Represents a cache record that has been locked, or soon will be locked.
// The record may exist locally or remotely.
class locked_cache_record
{
 public:
    virtual ~locked_cache_record() = default;
};

/*
 * A cache_record_lock holds a lock on zero or one record(s) in the
 * immutable cache.
 *
 * While one or more locks exist on a cache record, it won't be evicted. Thus,
 * a client that holds a lock can be assured that when it re-resolves the
 * corresponding request, the result will be immediately available.
 */
class cache_record_lock
{
 public:
    // The default constructor creates an object not having a lock.
    // The destructor releases the lock, if set, on the cache record.

    // Obtains a lock on the given cache record. Must not be called on an
    // object already having a lock.
    void
    set_record(std::unique_ptr<locked_cache_record> record);

 private:
    std::unique_ptr<locked_cache_record> record_;
};

} // namespace cradle

#endif
