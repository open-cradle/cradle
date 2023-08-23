#ifndef CRADLE_INNER_REQUESTS_TEST_CONTEXT_H
#define CRADLE_INNER_REQUESTS_TEST_CONTEXT_H

#include <cradle/inner/requests/generic.h>

namespace cradle {

// Context with hooks that are (only) useful for certain unit tests
class test_context_intf : public virtual context_intf
{
 public:
    virtual ~test_context_intf() = default;

    virtual void
    apply_fail_submit_async()
        = 0;

    virtual void
    apply_resolve_async_delay()
        = 0;
};

} // namespace cradle

#endif
