#include <cradle/plugins/secondary_cache/local/disk_mutable_store.h>

#include <coroutine>
#include <mutex>
#include <utility>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/storage/content_helpers.h>
#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

namespace cradle {

namespace {

// AC key used to address a mutable-store value in the shared underlying store;
// a namespace disjoint from durable-CAS content keys and AC association keys.
std::string
mutable_ac_key(std::string const& key)
{
    return "mut/" + key;
}

// Resumes the awaiting coroutine on a thread of the given write pool so the
// blocking on-disk write runs off the caller's thread.
struct write_pool_scheduler
{
    BS::thread_pool& pool;

    bool
    await_ready() const noexcept
    {
        return false;
    }

    void
    await_suspend(std::coroutine_handle<> handle) const
    {
        pool.detach_task([handle]() { handle.resume(); });
    }

    void
    await_resume() const noexcept
    {
    }
};

} // namespace

disk_mutable_store_impl::disk_mutable_store_impl(
    std::shared_ptr<local_durable_store> store, std::string name)
    : store_{std::move(store)}, name_{std::move(name)}
{
}

std::string const&
disk_mutable_store_impl::name() const
{
    return name_;
}

cppcoro::task<void>
disk_mutable_store_impl::put(std::string key, mutable_value value)
{
    co_await write_pool_scheduler{store_->write_pool()};
    auto& cache = store_->cache();
    auto const ac_key = mutable_ac_key(key);
    // Overwrite via delete-then-insert: plain insert no-ops on an existing
    // key, so any prior value is removed first. The mutex makes the compound
    // atomic with respect to concurrent operations on this instance. The
    // store's own digest argument is an internal dedup key over the value; the
    // value is small and stored inline.
    std::lock_guard<std::mutex> lock{overwrite_mutex_};
    if (auto ac_id = cache.look_up_ac_id(ac_key))
    {
        cache.remove_entry(*ac_id);
    }
    cache.insert(ac_key, digest_of(value), value);
    co_return;
}

cppcoro::task<std::optional<mutable_value>>
disk_mutable_store_impl::get(std::string key)
{
    co_await store_->read_pool().schedule();
    auto entry = store_->cache().find(mutable_ac_key(key));
    if (!entry)
    {
        // Not-present or reclaimed: a distinguishable miss, not an error.
        co_return std::nullopt;
    }
    // Values are always stored inline; an empty stored value is a hit with an
    // empty payload, distinct from a nullopt miss.
    co_return *entry->value;
}

cppcoro::task<bool>
disk_mutable_store_impl::exists(std::string key)
{
    co_await store_->read_pool().schedule();
    co_return store_->cache().look_up_ac_id(mutable_ac_key(key)).has_value();
}

std::unique_ptr<mutable_store_intf>
make_disk_mutable_store(
    std::shared_ptr<local_durable_store> store, std::string name)
{
    return std::make_unique<disk_mutable_store_impl>(
        std::move(store), std::move(name));
}

} // namespace cradle
