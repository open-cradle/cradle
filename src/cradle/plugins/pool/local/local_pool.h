#ifndef CRADLE_PLUGINS_POOL_LOCAL_LOCAL_POOL_H
#define CRADLE_PLUGINS_POOL_LOCAL_LOCAL_POOL_H

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

#include <BS_thread_pool.hpp>
#include <cppcoro/async_scope.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/pool/job_spec.h>
#include <cradle/inner/pool/job_types.h>
#include <cradle/inner/pool/pool_intf.h>

namespace cradle {

class ac_intf;
class cas_intf;
class inner_resources;
class mutable_store_intf;

// Resolved settings for a local pool, produced from service_config by the
// registration dispatcher.
struct pool_settings
{
    // Serialized-size boundary for input classification.
    std::size_t input_size_boundary;

    // Poll cadence bounds for the plugins-layer local_pool (from the
    // pool/poll_* keys). These configure the plugins local_pool and are not
    // wired into the inner observe loop; the inner observe loop uses
    // inner-side default backoff bounds instead.
    std::chrono::milliseconds poll_interval;
    std::chrono::milliseconds poll_max_interval;
};

// The realization of pool_intf that runs a job on the same machine. Holds
// references to already-registered CAS / AC / mutable-store instances (it does
// not own them) and dispatches work to a same-machine worker asynchronously.
class local_pool : public pool_intf
{
 public:
    local_pool(
        std::string name,
        inner_resources& resources,
        cas_intf& cas,
        ac_intf& ac,
        mutable_store_intf& mutable_store,
        pool_settings settings);

    ~local_pool();

    std::string const&
    name() const override;

    cas_intf&
    cas() override;

    ac_intf&
    ac() override;

    mutable_store_intf&
    mutable_store() override;

    // Assigns a random-UUID job id, records a queued job record under
    // jobs/JOB_ID, dispatches a local_worker to run the job asynchronously,
    // and returns the id. Throws on a genuine backend fault.
    cppcoro::task<job_id>
    submit(job_spec spec) override;

    // Records the cancelled terminal status when the job is not yet terminal;
    // otherwise a no-op. Does not interrupt in-flight work.
    cppcoro::task<void>
    cancel(job_id id) override;

 private:
    // Constructs a worker for one job and awaits its execution, owning the
    // worker for the duration through this coroutine's frame. Spawned by
    // submit onto the async thread pool as a fire-and-forget task.
    cppcoro::task<void>
    run_worker(job_spec spec, job_id id);

    std::string const name_;
    inner_resources& resources_;
    cas_intf& cas_;
    ac_intf& ac_;
    mutable_store_intf& mutable_store_;
    pool_settings settings_;
    cppcoro::async_scope async_scope_;
};

// Factory: constructs a local_pool bound to the given named storage instances,
// owned through the base interface for registration on inner_resources.
std::unique_ptr<pool_intf>
make_local_pool(
    std::string name,
    inner_resources& resources,
    cas_intf& cas,
    ac_intf& ac,
    mutable_store_intf& mutable_store,
    pool_settings settings);

} // namespace cradle

#endif
