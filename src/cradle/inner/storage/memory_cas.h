#ifndef CRADLE_INNER_STORAGE_MEMORY_CAS_H
#define CRADLE_INNER_STORAGE_MEMORY_CAS_H

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/digest.h>

namespace cradle {

// In-memory, std::map-backed content-addressable store. Thread-safe via an
// internal mutex (NFR-1). Suitable for tests (NFR-3); no external service,
// filesystem, or network dependency.
class memory_cas_impl : public cas_intf
{
 public:
    memory_cas_impl() = default;

    explicit memory_cas_impl(std::string name);

    std::string const&
    name() const override;

    cppcoro::task<void>
    put(digest key, blob content) override;

    cppcoro::task<std::optional<blob>>
    get(digest key) override;

    cppcoro::task<bool>
    exists(digest key) override;

 private:
    std::string const name_{"memory_cas"};
    mutable std::mutex mutex_;
    std::map<digest, blob> storage_;
};

// Factory helper: constructs a memory_cas_impl owned through the base
// interface, for registration on inner_resources.
std::unique_ptr<cas_intf>
make_memory_cas(std::string name);

} // namespace cradle

#endif
