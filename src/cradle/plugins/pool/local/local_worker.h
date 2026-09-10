#ifndef CRADLE_PLUGINS_POOL_LOCAL_LOCAL_WORKER_H
#define CRADLE_PLUGINS_POOL_LOCAL_LOCAL_WORKER_H

#include <memory>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/pool/job_spec.h>
#include <cradle/inner/pool/job_types.h>

namespace cradle {

class ac_intf;
class cas_intf;
class inner_resources;
class mutable_store_intf;

// Executes exactly one job against the storage instances a pool refers to and
// the provider machinery in inner_resources. Records running, loads inputs
// (inline directly, digest from the CAS), runs the provider via the seri
// registry, and on success writes the result to the CAS and the request-key to
// result-digest association to the AC before setting the terminal status. An
// interrupted or failed execution is set failed (or left non-terminal) and is
// never observable as succeeded.
class local_worker
{
 public:
    local_worker(
        inner_resources& resources,
        cas_intf& cas,
        ac_intf& ac,
        mutable_store_intf& mutable_store,
        std::string pool_name);

    // Runs one job to a terminal status. Does not throw for a provider failure
    // (that is recorded as a failed job record); may throw only on a genuine
    // backend fault while recording status.
    cppcoro::task<void>
    execute(job_spec spec, job_id id);

 private:
    inner_resources& resources_;
    cas_intf& cas_;
    ac_intf& ac_;
    mutable_store_intf& mutable_store_;
    std::string pool_name_;
};

// Factory: constructs a local_worker bound to the pool's storage references
// and resources.
std::unique_ptr<local_worker>
make_local_worker(
    inner_resources& resources,
    cas_intf& cas,
    ac_intf& ac,
    mutable_store_intf& mutable_store,
    std::string pool_name);

} // namespace cradle

#endif
