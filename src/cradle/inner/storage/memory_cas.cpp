#include <cradle/inner/storage/memory_cas.h>

#include <utility>

namespace cradle {

memory_cas_impl::memory_cas_impl(std::string name) : name_{std::move(name)}
{
}

std::string const&
memory_cas_impl::name() const
{
    return name_;
}

cppcoro::task<void>
memory_cas_impl::put(digest key, blob content)
{
    std::lock_guard<std::mutex> lock{mutex_};
    // Idempotent: insert only if absent; leave existing content unchanged.
    storage_.try_emplace(std::move(key), std::move(content));
    co_return;
}

cppcoro::task<std::optional<blob>>
memory_cas_impl::get(digest key)
{
    std::lock_guard<std::mutex> lock{mutex_};
    auto it = storage_.find(key);
    // A present entry (even an empty blob) is a hit, distinct from nullopt.
    co_return it != storage_.end() ? std::make_optional(it->second)
                                   : std::nullopt;
}

cppcoro::task<bool>
memory_cas_impl::exists(digest key)
{
    std::lock_guard<std::mutex> lock{mutex_};
    co_return storage_.find(key) != storage_.end();
}

std::unique_ptr<cas_intf>
make_memory_cas(std::string name)
{
    return std::make_unique<memory_cas_impl>(std::move(name));
}

} // namespace cradle
