#include <cradle/inner/storage/memory_ac.h>

#include <utility>

namespace cradle {

memory_ac_impl::memory_ac_impl(std::string name) : name_{std::move(name)}
{
}

std::string const&
memory_ac_impl::name() const
{
    return name_;
}

cppcoro::task<void>
memory_ac_impl::put(request_key key, digest value)
{
    std::lock_guard<std::mutex> lock{mutex_};
    // Idempotent: insert only if absent; leave an existing association
    // unchanged.
    storage_.try_emplace(std::move(key), std::move(value));
    co_return;
}

cppcoro::task<std::optional<digest>>
memory_ac_impl::get(request_key key)
{
    std::lock_guard<std::mutex> lock{mutex_};
    auto it = storage_.find(key);
    // A present entry is a hit, distinct from a nullopt miss.
    co_return it != storage_.end() ? std::make_optional(it->second)
                                   : std::nullopt;
}

cppcoro::task<bool>
memory_ac_impl::exists(request_key key)
{
    std::lock_guard<std::mutex> lock{mutex_};
    co_return storage_.find(key) != storage_.end();
}

std::unique_ptr<ac_intf>
make_memory_ac(std::string name)
{
    return std::make_unique<memory_ac_impl>(std::move(name));
}

} // namespace cradle
