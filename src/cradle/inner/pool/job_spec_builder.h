#ifndef CRADLE_INNER_POOL_JOB_SPEC_BUILDER_H
#define CRADLE_INNER_POOL_JOB_SPEC_BUILDER_H

#include <cstddef>
#include <string>
#include <vector>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/pool/job_spec.h>
#include <cradle/inner/pool/job_types.h>
#include <cradle/inner/storage/digest.h>

namespace cradle {

class cas_intf;

// Classify one already-serialized input against a size boundary. At or below
// the boundary yields an inline_input carrying the bytes; above the boundary
// the bytes are placed in cas (idempotently, via put_content) and a
// digest_input carrying the returned digest is yielded, ensuring the content
// is present before the job runs.
cppcoro::task<job_input>
build_job_input(cas_intf& cas, blob serialized_input, std::size_t boundary);

// Classify a sequence of already-serialized inputs against the same boundary,
// preserving argument order.
cppcoro::task<std::vector<job_input>>
build_job_inputs(
    cas_intf& cas, std::vector<blob> serialized_inputs, std::size_t boundary);

// Assemble a job_spec from a provider identity, a request key, a context
// identifier, and a set of already-serialized inputs, classifying each input
// against the boundary. Placement of digest-conveyed content in cas happens
// during construction.
cppcoro::task<job_spec>
build_job_spec(
    cas_intf& cas,
    provider_id provider,
    request_key key,
    context_id context,
    std::vector<blob> serialized_inputs,
    std::size_t boundary);

} // namespace cradle

#endif
