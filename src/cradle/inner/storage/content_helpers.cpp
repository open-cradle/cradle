#include <cradle/inner/storage/content_helpers.h>

#include <utility>

#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/utilities/errors.h>

namespace cradle {

digest
digest_of(blob const& content)
{
    // Same hashing mechanism local_disk_cache uses to digest a blob, so
    // digests stay consistent across the codebase.
    return get_unique_string_tmpl(content);
}

cppcoro::task<digest>
put_content(cas_intf& cas, blob content)
{
    // No digest supplied, so create it first.
    digest d = digest_of(content);
    co_await cas.put(d, std::move(content));
    co_return d;
}

cppcoro::task<void>
put_content(cas_intf& cas, digest key, blob content)
{
    // Caller-supplied digest; a no-op when already present.
    co_await cas.put(std::move(key), std::move(content));
    co_return;
}

cppcoro::task<void>
put_content_verified(cas_intf& cas, blob content, digest expected)
{
    // Recompute and compare digest; a mismatch is a trust-boundary fault.
    if (digest_of(content) != expected)
    {
        CRADLE_THROW(
            internal_check_failed() << internal_error_message_info(
                "put_content_verified: content does not match expected "
                "digest"));
    }
    co_await cas.put(std::move(expected), std::move(content));
    co_return;
}

} // namespace cradle
