#ifndef CRADLE_INNER_REQUESTS_TYPES_H
#define CRADLE_INNER_REQUESTS_TYPES_H

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

} // namespace cradle

#endif
