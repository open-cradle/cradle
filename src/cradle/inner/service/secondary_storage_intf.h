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

    // Reads the value for key.
    // Returns std::nullopt if the value is not in the storage.
    // Throws on other errors.
    // This could be a coroutine so takes arguments by value.
    virtual cppcoro::task<std::optional<blob>>
    read(std::string key) = 0;

    // This operation may be synchronous or asynchronous.
    // If synchronous, it will throw on errors.
    // This could be a coroutine so takes arguments by value.
    virtual cppcoro::task<void>
    write(std::string key, blob value) = 0;
};

} // namespace cradle

#endif
