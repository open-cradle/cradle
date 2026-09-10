#ifndef CRADLE_INNER_RESOLVE_RESOLVE_POOL_H
#define CRADLE_INNER_RESOLVE_RESOLVE_POOL_H

#include <cstddef>
#include <vector>

#include <cppcoro/task.hpp>

#include <cradle/inner/pool/job_types.h>
#include <cradle/inner/storage/digest.h>

namespace cradle {

class ac_intf;
class cas_intf;
class local_context_intf;
class mutable_store_intf;
class pool_intf;
struct job_record;

// Observe a job's per-job record on the pool's mutable store until it reaches
// a terminal status, using a bounded-backoff poll. Returns the terminal
// record. Throws on a genuine backend fault. The context supplies the
// cancellable coroutine delay used between polls, so the poll never blocks a
// pool thread.
cppcoro::task<job_record>
observe_until_terminal(local_context_intf& ctx, pool_intf& pool, job_id id);

// The job-path branch for one leaf. When the pool's AC already associates the
// request key with a digest, returns the content by that digest without
// submitting a job (the AC-hit short-circuit). Otherwise builds the job spec
// (classifying inputs into the pool's CAS), submits it, observes to terminal,
// and on success returns the result content by its digest; on a failed or
// cancelled terminal state, throws surfacing the recorded error or
// cancellation. Returns the serialized result content as a blob, which the
// caller deserializes into the leaf's value type.
cppcoro::task<blob>
resolve_leaf_on_pool(
    local_context_intf& ctx,
    pool_intf& pool,
    provider_id provider,
    request_key key,
    context_id context,
    std::vector<blob> serialized_inputs,
    std::size_t input_size_boundary);

} // namespace cradle

#endif
