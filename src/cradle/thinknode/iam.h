#ifndef CRADLE_THINKNODE_IAM_H
#define CRADLE_THINKNODE_IAM_H

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/thinknode/types.hpp>
#include <cradle/typing/service/core.h>

namespace cradle {

cppcoro::task<string>
get_context_id(thinknode_request_context ctx, string realm);

// Query the contents of a context.
cppcoro::shared_task<thinknode_context_contents>
get_context_contents(thinknode_request_context ctx, string context_id);

cppcoro::shared_task<string>
get_context_bucket(thinknode_request_context ctx, string context_id);

} // namespace cradle

#endif
