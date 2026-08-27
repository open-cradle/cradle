#ifndef CRADLE_INNER_STORAGE_MUTABLE_STORE_INTF_H
#define CRADLE_INNER_STORAGE_MUTABLE_STORE_INTF_H

#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/storage/digest.h>

namespace cradle {

// Generic mutable store: a mapping of key -> opaque value, with overwrite.
// Carries no built-in interpretation of the value structure (FR-19).
class mutable_store_intf
{
 public:
    virtual ~mutable_store_intf() = default;

    // Name of this store instance, used for named-instance selection.
    virtual std::string const&
    name() const
        = 0;

    // Associate an opaque value with a key (FR-14). A later put under an
    // existing key replaces the prior value (FR-16, overwrite).
    // Arguments are taken by value because this may be a coroutine.
    // Throws on a genuine backend error (D4).
    virtual cppcoro::task<void>
    put(std::string key, mutable_value value)
        = 0;

    // Retrieve the value most recently stored under a key (FR-15).
    // Returns std::nullopt for a not-present key (FR-17, a distinguishable
    // miss); throws on a genuine backend error (D4).
    virtual cppcoro::task<std::optional<mutable_value>>
    get(std::string key)
        = 0;

    // Report whether a key is currently present (FR-18).
    // Throws only on a genuine backend error.
    virtual cppcoro::task<bool>
    exists(std::string key)
        = 0;
};

} // namespace cradle

#endif
