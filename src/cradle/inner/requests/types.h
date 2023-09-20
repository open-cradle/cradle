#ifndef CRADLE_INNER_REQUESTS_TYPES_H
#define CRADLE_INNER_REQUESTS_TYPES_H

#include <cstddef>
#include <string>

namespace cradle {

// Status of an asynchronous operation: a (cppcoro) task, associated
// with a coroutine.
// CANCELLED, FINISHED and ERROR are final statuses: once a task ends up in
// one of these, its status won't change anymore.
enum class async_status
{
    CREATED, // Task was created
    SUBS_RUNNING, // Subtasks running, main task waiting for them
    SELF_RUNNING, // Subtasks finished, main task running
    CANCELLED, // Cancellation completed
    AWAITING_RESULT, // Calculation completed, but the result still has to be
                     // stored in the context (transient internal status)
    FINISHED, // Finished successfully
    ERROR // Ended due to error
};

std::string
to_string(async_status s);

// Identifies an async operation. Unique within the context of its
// (local or remote) service.
using async_id = uint64_t;

static constexpr async_id NO_ASYNC_ID{~async_id{}};

/*
 * Id for a catalog instance.
 *
 * The intention is to have a unique id for the catalog in a newly loaded
 * DLL. When the same DLL is loaded again, a distinct id will be allocated.
 * When a DLL is unloaded, all corresponding cereal_functions_registry::entry_t
 * objects should be removed, but exceptions that are not handled properly
 * could interfere. If so, we could be left with stale pointers inside entries
 * with a stale id. By reallocating a new id when the DLL is reloaded, these
 * stale entries be ignored; overwriting them tends to lead to crashes.
 */
class catalog_id
{
 public:
    using wrapped_t = std::size_t;

    catalog_id() = default;

    // Get an id for a catalog that is not inside a DLL.
    // TODO temporary catalogs in unit tests should also have a DLL-like id?!
    static catalog_id
    get_static_id();

    // Allocate an id for a catalog that is inside a DLL.
    static catalog_id
    alloc_dll_id();

    wrapped_t
    value() const
    {
        return wrapped_;
    }

    bool
    is_valid() const
    {
        return wrapped_ >= STATIC_ID;
    }

    bool
    is_valid_dll() const
    {
        return wrapped_ >= MIN_DLL_ID;
    }

    bool
    operator==(catalog_id const& other) const
    {
        return wrapped_ == other.wrapped_;
    }

    bool
    operator!=(catalog_id const& other) const
    {
        return !(*this == other);
    }

 private:
    static constexpr wrapped_t NO_ID{0};
    static constexpr wrapped_t STATIC_ID{1};
    static constexpr wrapped_t MIN_DLL_ID{2};

    wrapped_t wrapped_{NO_ID};

    catalog_id(wrapped_t wrapped) : wrapped_{wrapped}
    {
    }
};

} // namespace cradle

#endif
