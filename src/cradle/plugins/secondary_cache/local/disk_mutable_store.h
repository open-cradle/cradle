#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_MUTABLE_STORE_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_LOCAL_DISK_MUTABLE_STORE_H

#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/mutable_store_intf.h>

namespace cradle {

class local_durable_store;

// Durable generic mutable store: a persisted key -> opaque value mapping with
// overwrite, over the reused on-disk store. Values are stored inline (assumed
// small). Carries no interpretation of the value structure.
class disk_mutable_store_impl : public mutable_store_intf
{
 public:
    disk_mutable_store_impl(
        std::shared_ptr<local_durable_store> store, std::string name);

    std::string const&
    name() const override;

    // Persist an opaque value under a key. A later put under an existing key
    // replaces the prior value for all subsequent read-backs, including across
    // sessions. The overwrite is atomic with respect to concurrent operations
    // on this instance. Throws on a genuine backend fault.
    cppcoro::task<void>
    put(std::string key, mutable_value value) override;

    // Retrieve the value most recently stored under a key. Returns
    // std::nullopt for a not-present or reclaimed key.
    cppcoro::task<std::optional<mutable_value>>
    get(std::string key) override;

    // Report whether a key is currently present.
    cppcoro::task<bool>
    exists(std::string key) override;

 private:
    std::shared_ptr<local_durable_store> store_;
    std::string const name_;
    std::mutex overwrite_mutex_;
};

// Factory: constructs a disk_mutable_store_impl over a shared durable store.
std::unique_ptr<mutable_store_intf>
make_disk_mutable_store(
    std::shared_ptr<local_durable_store> store, std::string name);

} // namespace cradle

#endif
