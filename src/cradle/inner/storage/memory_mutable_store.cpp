#include <cradle/inner/storage/memory_mutable_store.h>

#include <utility>

namespace cradle {

memory_mutable_store_impl::memory_mutable_store_impl(std::string name)
    : name_{std::move(name)}
{
}

std::string const&
memory_mutable_store_impl::name() const
{
    return name_;
}

cppcoro::task<void>
memory_mutable_store_impl::put(std::string key, mutable_value value)
{
    std::lock_guard<std::mutex> lock{mutex_};
    // Overwrite: a later put under an existing key replaces the prior
    // value, so the most recent write wins.
    storage_.insert_or_assign(std::move(key), std::move(value));
    co_return;
}

cppcoro::task<std::optional<mutable_value>>
memory_mutable_store_impl::get(std::string key)
{
    std::lock_guard<std::mutex> lock{mutex_};
    auto it = storage_.find(key);
    // Membership decides hit vs. miss, so an empty value is still a hit
    // distinct from a nullopt miss.
    co_return it != storage_.end() ? std::make_optional(it->second)
                                   : std::nullopt;
}

cppcoro::task<bool>
memory_mutable_store_impl::exists(std::string key)
{
    std::lock_guard<std::mutex> lock{mutex_};
    co_return storage_.find(key) != storage_.end();
}

std::unique_ptr<mutable_store_intf>
make_memory_mutable_store(std::string name)
{
    return std::make_unique<memory_mutable_store_impl>(std::move(name));
}

} // namespace cradle
