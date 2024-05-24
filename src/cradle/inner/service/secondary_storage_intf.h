#ifndef CRADLE_INNER_SERVICE_SECONDARY_STORAGE_INTF_H
#define CRADLE_INNER_SERVICE_SECONDARY_STORAGE_INTF_H

// Interface to a secondary storage (e.g., a disk cache).
// The implementation will be provided by a plugin.

#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>

namespace cradle {

class secondary_storage_intf
{
 public:
    virtual ~secondary_storage_intf() = default;

    // Clear the cache, removing all entries.
    virtual void
    clear()
        = 0;

    // Reads the serialized value for key.
    // Returns std::nullopt if the value is not in the storage;
    // throws on other errors.
    // This could be a coroutine so takes arguments by value.
    virtual cppcoro::task<std::optional<blob>>
    read(std::string key) = 0;

    // Writes a serialized value under the given key.
    // This operation may be synchronous or asynchronous.
    // If synchronous, it will throw on errors.
    // This could be a coroutine so takes arguments by value.
    virtual cppcoro::task<void>
    write(std::string key, blob value) = 0;

    // Returns true if this storage medium allows a serialized value to
    // contain references to blob files.
    // If this returns false, a write() caller should ensure that any blob
    // files _inside_ the to-be-written value have been expanded. The value
    // itself can still be a blob file, and if so the write() implementation
    // should interpret it as a byte sequence, disregarding the blob file
    // aspect.
    virtual bool
    allow_blob_files() const
        = 0;
};

} // namespace cradle

#endif
