#ifndef CRADLE_INNER_STORAGE_MEMORY_AC_H
#define CRADLE_INNER_STORAGE_MEMORY_AC_H

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/digest.h>

namespace cradle {

// In-memory, std::map-backed action cache. Thread-safe via an internal
// mutex (NFR-1). Suitable for tests (NFR-3); no external service,
// filesystem, or network dependency.
class memory_ac_impl : public ac_intf
{
 public:
    memory_ac_impl() = default;

    explicit memory_ac_impl(std::string name);

    std::string const&
    name() const override;

    cppcoro::task<void>
    put(request_key key, digest value) override;

    cppcoro::task<std::optional<digest>>
    get(request_key key) override;

    cppcoro::task<bool>
    exists(request_key key) override;

 private:
    std::string const name_{"memory_ac"};
    mutable std::mutex mutex_;
    std::map<request_key, digest> storage_;
};

// Factory helper: constructs a memory_ac_impl owned through the base
// interface, for registration on inner_resources.
std::unique_ptr<ac_intf>
make_memory_ac(std::string name);

} // namespace cradle

#endif
