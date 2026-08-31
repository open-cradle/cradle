#ifndef CRADLE_INNER_STORAGE_CONTENT_HELPERS_H
#define CRADLE_INNER_STORAGE_CONTENT_HELPERS_H

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/digest.h>

namespace cradle {

// Compute a digest over content, independently of storing it.
// Hashing is done here, outside the CAS; not a coroutine (pure computation).
digest
digest_of(blob const& content);

// Compute the digest of content, store it idempotently, and return the
// digest. Returns the digest even when it was already present.
cppcoro::task<digest>
put_content(cas_intf& cas, blob content);

// Store content only when its digest is not already present; a no-op when the
// digest is already present. Uses the caller-supplied digest.
cppcoro::task<void>
put_content_if_absent(cas_intf& cas, digest key, blob content);

// Verify content against the supplied digest, then store on a match.
// Throws on a digest mismatch.
cppcoro::task<void>
put_content_verified(cas_intf& cas, blob content, digest expected);

} // namespace cradle

#endif
