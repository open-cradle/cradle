#ifndef CRADLE_INNER_POOL_JOB_RECORD_H
#define CRADLE_INNER_POOL_JOB_RECORD_H

#include <cstdint>
#include <string>

#include <cradle/inner/pool/job_types.h>
#include <cradle/inner/storage/digest.h>

namespace cradle {

// The lifecycle states of a job. queued and running are non-terminal;
// succeeded, failed, and cancelled are terminal. Once terminal, a job's status
// is not observed to leave that state. Serialized as its integer value; no
// preprocessor annotation.
enum class job_status : std::uint8_t
{
    queued = 0,
    running = 1,
    succeeded = 2,
    failed = 3,
    cancelled = 4
};

// True for the three terminal states, false for queued and running. Lets the
// resolve branch stop observing once a terminal state is reached.
bool
is_terminal(job_status status);

// The per-job status record stored as opaque bytes under a per-job key. The
// output digest is meaningful only when status is succeeded; the error is
// meaningful only when status is failed. Both are empty otherwise.
struct job_record
{
    job_id id;
    request_key key;
    std::string pool_name;
    job_status status{job_status::queued};

    // Result digest, populated only on succeeded; empty otherwise.
    digest output_digest;

    // Human-readable error description, populated only on failed; empty
    // otherwise.
    std::string error;
};

// The mutable-store key convention for a job's record: the id prefixed with
// "jobs/". Reading this key yields the job's most recently recorded status.
std::string
job_record_key(job_id const& id);

// Encode a job record to opaque bytes for the mutable store. Current
// mechanism: msgpack. The mutable store carries no interpretation of these
// bytes.
mutable_value
serialize_job_record(job_record const& record);

// Decode a job record from opaque mutable-store bytes. Throws on malformed
// bytes (a genuine fault, not a miss).
job_record
deserialize_job_record(mutable_value const& value);

} // namespace cradle

#endif
