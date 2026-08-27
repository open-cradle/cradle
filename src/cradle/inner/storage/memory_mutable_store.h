#ifndef CRADLE_INNER_STORAGE_MEMORY_MUTABLE_STORE_H
#define CRADLE_INNER_STORAGE_MEMORY_MUTABLE_STORE_H

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/mutable_store_intf.h>

namespace cradle {

// In-memory, std::map-backed mutable store. Thread-safe via an internal
// mutex (NFR-1). Suitable for tests (NFR-3); no external service,
// filesystem, or network dependency.
class memory_mutable_store_impl : public mutable_store_intf
{
 public:
    memory_mutable_store_impl() = default;

    explicit memory_mutable_store_impl(std::string name);

    std::string const&
    name() const override;

    cppcoro::task<void>
    put(std::string key, mutable_value value) override;

    cppcoro::task<std::optional<mutable_value>>
    get(std::string key) override;

    cppcoro::task<bool>
    exists(std::string key) override;

 private:
    std::string const name_{"memory_mutable_store"};
    mutable std::mutex mutex_;
    std::map<std::string, mutable_value> storage_;
};

// Factory helper: constructs a memory_mutable_store_impl owned through the
// base interface, for registration on inner_resources.
std::unique_ptr<mutable_store_intf>
make_memory_mutable_store(std::string name);

} // namespace cradle

#endif
