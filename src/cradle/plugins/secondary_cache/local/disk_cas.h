#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_CAS_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_CAS_H

#include <memory>
#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/digest.h>

namespace cradle {

class local_durable_store;

// Durable content-addressable store: an idempotent digest -> bytes mapping
// persisted across sessions over the reused on-disk store. Never computes a
// digest; the caller always supplies it. Verifies retrieved content against
// its digest so corrupt/truncated content is never returned as valid.
class disk_cas_impl : public cas_intf
{
 public:
    disk_cas_impl(
        std::shared_ptr<local_durable_store> store, std::string name);

    std::string const&
    name() const override;

    // Idempotently persist content under a caller-supplied digest. A repeat
    // put of an already-present digest is a no-op, including across sessions.
    // Large content is stored in an external lz4 file via a two-phase insert;
    // small content is stored inline. Throws on a genuine backend fault.
    cppcoro::task<void>
    put(digest key, blob content) override;

    // Retrieve content previously stored under a digest. Returns std::nullopt
    // for a not-present or reclaimed digest. Throws on an integrity/truncation
    // failure at the trust boundary and on a genuine backend fault.
    cppcoro::task<std::optional<blob>>
    get(digest key) override;

    // Report whether a digest is currently present, without returning content.
    // A reclaimed digest reports absent.
    cppcoro::task<bool>
    exists(digest key) override;

 private:
    std::shared_ptr<local_durable_store> store_;
    std::string const name_;
};

// Factory: constructs a disk_cas_impl over a shared durable store, owned
// through the base interface for registration on inner_resources.
std::unique_ptr<cas_intf>
make_disk_cas(std::shared_ptr<local_durable_store> store, std::string name);

} // namespace cradle

#endif
