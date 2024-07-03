#ifndef CRADLE_INNER_REQUESTS_TEST_CONTEXT_H
#define CRADLE_INNER_REQUESTS_TEST_CONTEXT_H

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/config.h>

namespace cradle {

// Context mixin setting parameters used by certain unit tests.
// Intended for remote execution; to be inherited by a context on the client
// side:
// - A remote context will update the config map sent to the remote;
// - A local context will copy the params to a remote context set as its
//   delegate.
class test_params_context_mixin
{
 public:
    test_params_context_mixin() = default;

    test_params_context_mixin(service_config const& config);

    // Causes submit_async to fail on the remote
    void
    fail_submit_async()
    {
        fail_submit_async_ = true;
    }

    // Sets the delay (in ms) that a submit_async call will wait on the
    // remote, before returning the remote_id.
    void
    set_submit_async_delay(int delay)
    {
        submit_async_delay_ = delay;
    }

    // Sets the delay (in ms) that a resolve_async operation / thread will wait
    // after starting.
    // By extending / aggerating the existing short delay, the corresponding
    // race condition becomes reproducible and can be checked in a unit test.
    void
    set_resolve_async_delay(int delay)
    {
        resolve_async_delay_ = delay;
    }

    // Sets the delay (in ms) that a set_result() call will wait before
    // actually setting the result.
    // By extending / aggerating the existing short delay, the corresponding
    // race condition becomes reproducible and can be checked in a unit test.
    void
    set_set_result_delay(int delay)
    {
        set_result_delay_ = delay;
    }

    // Copy this object's parameters to other
    void
    copy_test_params(test_params_context_mixin& other) const;

    void
    update_config_map_with_test_params(service_config_map& config_map) const;

    void
    load_from_config(service_config const& config);

 protected:
    bool fail_submit_async_{false};
    int submit_async_delay_{0};
    int resolve_async_delay_{0};
    int set_result_delay_{0};
};

// Context hooks that are (only) useful for certain unit tests.
// Intended for remote execution; to be implemented by a local context on the
// server side.
class test_context_intf
{
 public:
    virtual ~test_context_intf() = default;

    virtual void
    apply_fail_submit_async()
        = 0;

    virtual void
    apply_submit_async_delay()
        = 0;

    virtual void
    apply_resolve_async_delay()
        = 0;
};

} // namespace cradle

#endif
