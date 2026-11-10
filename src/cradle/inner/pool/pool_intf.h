#ifndef CRADLE_INNER_POOL_POOL_INTF_H
#define CRADLE_INNER_POOL_POOL_INTF_H

#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/pool/job_spec.h>
#include <cradle/inner/pool/job_types.h>

namespace cradle {

class cas_intf;
class ac_intf;
class mutable_store_intf;

// A named execution backend. Exposes the CAS / AC / mutable store it refers to
// (it does not own them), accepts a job for execution and returns an
// identifier for it, and accepts a best-effort cancellation request. Status is
// observable only through the referred-to mutable store; there is no wait API.
class pool_intf
{
 public:
    virtual ~pool_intf() = default;

    // Name by which this pool is identified and selected.
    virtual std::string const&
    name() const
        = 0;

    // The content-addressable store this pool refers to. Not owned by the
    // pool.
    virtual cas_intf&
    cas()
        = 0;

    // The action cache this pool refers to. Not owned by the pool.
    virtual ac_intf&
    ac() = 0;

    // The mutable store this pool refers to. Not owned by the pool. This is
    // the store under which per-job status records are observable.
    virtual mutable_store_intf&
    mutable_store()
        = 0;

    // Accept a job for execution and return an identifier for it. Records a
    // queued job record and dispatches the work. The returned identifier is
    // usable to observe the job's status and to request its cancellation.
    // Throws on a genuine backend fault.
    virtual cppcoro::task<job_id>
    submit(job_spec spec) = 0;

    // Request best-effort cancellation of a previously accepted job. When the
    // job is not yet terminal, causes it to become observable as cancelled; it
    // doesn't necessarily interrupt work already in progress. A cancel of an
    // already-terminal or unknown job is a no-op. Throws on a genuine backend
    // fault.
    virtual cppcoro::task<void>
    cancel(job_id id) = 0;
};

} // namespace cradle

#endif
