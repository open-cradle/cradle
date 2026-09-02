#include <cradle/plugins/secondary_cache/local/disk_ac.h>

#include <coroutine>
#include <utility>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/storage/content_helpers.h>
#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

namespace cradle {

namespace {

// AC key used to address a request-key association in the shared underlying
// store; a namespace disjoint from durable-CAS content keys.
std::string
ac_ac_key(request_key const& key)
{
    return "ac/" + key;
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

disk_ac_impl::disk_ac_impl(
    std::shared_ptr<local_durable_store> store, std::string name)
    : store_{std::move(store)}, name_{std::move(name)}
{
}

std::string const&
disk_ac_impl::name() const
{
    return name_;
}

cppcoro::task<void>
disk_ac_impl::put(request_key key, digest value)
{
    co_await write_pool_scheduler{store_->write_pool()};
    // The association's semantic digest is stored as a small inline value; the
    // underlying store's own digest argument is an internal dedup key over
    // that value, keeping AC rows in a hash space disjoint from CAS content
    // rows.
    auto stored_value = make_blob(std::move(value));
    store_->cache().insert(
        ac_ac_key(key), digest_of(stored_value), stored_value);
    co_return;
}

cppcoro::task<std::optional<digest>>
disk_ac_impl::get(request_key key)
{
    co_await store_->read_pool().schedule();
    auto entry = store_->cache().find(ac_ac_key(key));
    if (!entry)
    {
        // Not-associated or reclaimed: a distinguishable miss, not an error.
        co_return std::nullopt;
    }
    // AC values are small and always stored inline.
    co_return to_string(*entry->value);
}

cppcoro::task<bool>
disk_ac_impl::exists(request_key key)
{
    co_await store_->read_pool().schedule();
    co_return store_->cache().look_up_ac_id(ac_ac_key(key)).has_value();
}

std::unique_ptr<ac_intf>
make_disk_ac(std::shared_ptr<local_durable_store> store, std::string name)
{
    return std::make_unique<disk_ac_impl>(std::move(store), std::move(name));
}

} // namespace cradle
