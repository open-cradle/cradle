#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_AC_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_AC_H

#include <memory>
#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/digest.h>

namespace cradle {

class local_durable_store;

// Durable action cache: a persisted request-key -> result-digest association
// over the reused on-disk store. The digest is a small value stored inline
// under the request key; retrieval reads the stored value back and returns it
// as the digest. Put is insert-if-absent, matching the M1 in-memory AC.
class disk_ac_impl : public ac_intf
{
 public:
    disk_ac_impl(std::shared_ptr<local_durable_store> store, std::string name);

    std::string const&
    name() const override;

    // Persist an association from a request key to a result digest.
    // Insert-if-absent: an existing association is left unchanged. Throws on a
    // genuine backend fault.
    cppcoro::task<void>
    put(request_key key, digest value) override;

    // Retrieve the digest previously associated with a request key. Returns
    // std::nullopt for a not-associated or reclaimed key.
    cppcoro::task<std::optional<digest>>
    get(request_key key) override;

    // Report whether a request key currently has an association.
    cppcoro::task<bool>
    exists(request_key key) override;

 private:
    std::shared_ptr<local_durable_store> store_;
    std::string const name_;
};

// Factory: constructs a disk_ac_impl over a shared durable store, owned
// through the base interface for registration on inner_resources.
std::unique_ptr<ac_intf>
make_disk_ac(std::shared_ptr<local_durable_store> store, std::string name);

} // namespace cradle

#endif
