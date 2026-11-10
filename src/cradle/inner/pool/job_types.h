#ifndef CRADLE_INNER_POOL_JOB_TYPES_H
#define CRADLE_INNER_POOL_JOB_TYPES_H

#include <string>
#include <variant>

#include <cradle/inner/core/type_definitions.h> // provides cradle::blob
#include <cradle/inner/storage/digest.h> // digest, request_key

namespace cradle {

// Identifier a pool assigns to an accepted job, usable to observe and cancel
// the job. A random UUID string for the local pool; unique without
// coordination and uniform across pools and hosts.
using job_id = std::string;

// Identity of the provider to run: the uuid string under which a resolver is
// registered in the seri_registry. A plain std::string for parity with how the
// registry keys resolvers.
using provider_id = std::string;

// Context identifier carried by a job. May be empty for M2; nothing on the M2
// worker path requires a populated value.
using context_id = std::string;

// One job input conveyed directly: its serialized bytes travel within the job.
struct inline_input
{
    blob bytes;
};

// One job input conveyed by content digest: the job carries a digest naming
// content already present in the pool's CAS. The content is placed in the CAS
// before the job runs.
struct digest_input
{
    digest content_digest;
};

// A single job input, conveyed either directly or by digest. The choice is
// made by the input classifier against a configurable size boundary.
using job_input = std::variant<inline_input, digest_input>;

} // namespace cradle

#endif
