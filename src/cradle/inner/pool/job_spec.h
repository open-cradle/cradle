#ifndef CRADLE_INNER_POOL_JOB_SPEC_H
#define CRADLE_INNER_POOL_JOB_SPEC_H

#include <vector>

#include <cradle/inner/pool/job_types.h>
#include <cradle/inner/storage/digest.h>

namespace cradle {

// The flat description of one leaf calculation to run as a job. Constructed by
// the job-spec builder and passed to pool_intf::submit. Each input is conveyed
// either directly or by digest (job_input); digest-conveyed content is present
// in the pool's CAS before the job runs.
struct job_spec
{
    // Identity of the provider to run.
    provider_id provider;

    // Stable string identity of the leaf this job computes; the key under
    // which the result-digest association is recorded in the AC.
    request_key key;

    // Context identifier for the job; may be empty for M2.
    context_id context;

    // The leaf's inputs, in argument order, each classified inline-or-digest.
    std::vector<job_input> inputs;
};

} // namespace cradle

#endif
