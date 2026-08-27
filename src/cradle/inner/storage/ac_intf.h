#ifndef CRADLE_INNER_STORAGE_AC_INTF_H
#define CRADLE_INNER_STORAGE_AC_INTF_H

#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/storage/digest.h>

namespace cradle {

// Action cache: a mapping of request key -> result digest.
class ac_intf
{
 public:
    virtual ~ac_intf() = default;

    // Name of this store instance, used for named-instance selection.
    virtual std::string const&
    name() const
        = 0;

    // Record an association from a request key to a result digest (FR-5).
    // Arguments are taken by value because this may be a coroutine.
    // Throws on a genuine backend error (D4).
    virtual cppcoro::task<void>
    put(request_key key, digest value)
        = 0;

    // Retrieve the digest previously associated with a request key (FR-6).
    // Returns std::nullopt for a not-associated key (FR-7, a distinguishable
    // miss); throws on a genuine backend error (D4).
    virtual cppcoro::task<std::optional<digest>>
    get(request_key key)
        = 0;

    // Report whether a request key is currently associated with a digest
    // (FR-8). Throws only on a genuine backend error.
    virtual cppcoro::task<bool>
    exists(request_key key)
        = 0;
};

} // namespace cradle

#endif
