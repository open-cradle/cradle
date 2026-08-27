#ifndef CRADLE_INNER_STORAGE_CAS_INTF_H
#define CRADLE_INNER_STORAGE_CAS_INTF_H

#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/storage/digest.h>

namespace cradle {

// Content-addressable store: an idempotent mapping of digest -> bytes.
// Never computes a digest; the caller always supplies it (FR-13).
class cas_intf
{
 public:
    virtual ~cas_intf() = default;

    // Name of this store instance, used for named-instance selection.
    virtual std::string const&
    name() const
        = 0;

    // Idempotently store content under a caller-supplied digest.
    // Storing an already-present digest is a no-op and not an error (FR-1).
    // Arguments are taken by value because this may be a coroutine.
    // Throws on a genuine backend error (D4).
    virtual cppcoro::task<void>
    put(digest key, blob content)
        = 0;

    // Retrieve the content previously stored under a digest (FR-2).
    // Returns std::nullopt for a not-present digest (FR-3, a distinguishable
    // miss); throws on a genuine backend error (D4).
    virtual cppcoro::task<std::optional<blob>>
    get(digest key)
        = 0;

    // Report whether a digest is currently present, without returning the
    // content (FR-4). Throws only on a genuine backend error.
    virtual cppcoro::task<bool>
    exists(digest key)
        = 0;
};

} // namespace cradle

#endif
