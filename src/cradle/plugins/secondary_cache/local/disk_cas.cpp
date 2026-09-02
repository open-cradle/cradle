#include <cradle/plugins/secondary_cache/local/disk_cas.h>

#include <coroutine>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <boost/numeric/conversion/cast.hpp>

#include <fmt/format.h>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/lz4.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/storage/content_helpers.h>
#include <cradle/plugins/secondary_cache/local/local_durable_store.h>

namespace cradle {

namespace {

// Raised when retrieved external content fails its integrity check (truncation
// or digest mismatch) so corrupt content is surfaced as an error, never as a
// valid value.
class disk_cas_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

// AC key used to address CAS content in the shared underlying store.
std::string
cas_ac_key(digest const& key)
{
    return "cas/" + key;
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

// Content at or below this size is stored inline in the index rather than in
// an external file; mirrors the reused disk-cache threshold.
constexpr std::size_t inline_size_threshold = 1024;

} // namespace

disk_cas_impl::disk_cas_impl(
    std::shared_ptr<local_durable_store> store, std::string name)
    : store_{std::move(store)}, name_{std::move(name)}
{
}

std::string const&
disk_cas_impl::name() const
{
    return name_;
}

cppcoro::task<void>
disk_cas_impl::put(digest key, blob content)
{
    co_await write_pool_scheduler{store_->write_pool()};
    auto& cache = store_->cache();
    auto const ac_key = cas_ac_key(key);
    // A value goes in an external file only if it is big enough and is not
    // already backed by a blob file. The caller-supplied digest is used
    // verbatim; the content is never re-hashed.
    if (content.size() > inline_size_threshold
        && !content.mapped_file_data_owner())
    {
        auto optional_cas_id = cache.initiate_insert(ac_key, key);
        // A nullopt cas_id means the entry is already present: idempotent
        // no-op.
        if (optional_cas_id)
        {
            auto cas_id = *optional_cas_id;
            auto max_compressed_size
                = lz4::max_compressed_size(content.size());
            byte_vector compressed(max_compressed_size);
            auto actual_compressed_size = lz4::compress(
                compressed.data(),
                max_compressed_size,
                content.data(),
                content.size());
            {
                auto path = cache.get_path_for_digest(key);
                std::ofstream output;
                open_file(
                    output,
                    path,
                    std::ios::out | std::ios::trunc | std::ios::binary);
                output.write(
                    reinterpret_cast<char const*>(compressed.data()),
                    actual_compressed_size);
            }
            cache.finish_insert(
                cas_id, actual_compressed_size, content.size());
        }
    }
    else
    {
        // insert is a no-op when the AC key is already present: idempotent.
        cache.insert(ac_key, key, content);
    }
    co_return;
}

cppcoro::task<std::optional<blob>>
disk_cas_impl::get(digest key)
{
    co_await store_->read_pool().schedule();
    auto& cache = store_->cache();
    auto entry = cache.find(cas_ac_key(key));
    if (!entry)
    {
        // Not-present or reclaimed: a distinguishable miss, not an error.
        co_return std::nullopt;
    }
    if (entry->value)
    {
        co_return *entry->value;
    }
    // External file: read, decompress, and verify integrity.
    auto path = cache.get_path_for_digest(entry->digest);
    auto data = read_file_contents(path);
    auto original_size
        = boost::numeric_cast<std::size_t>(entry->original_size);
    byte_vector decompressed(original_size);
    auto decompressed_size = lz4::decompress(
        decompressed.data(), original_size, data.data(), data.size());
    // A truncated (interrupted) write yields a short decompression; reject it.
    if (decompressed_size != original_size)
    {
        throw disk_cas_error(fmt::format(
            "decompression gave {} bytes, expected {}",
            decompressed_size,
            original_size));
    }
    auto result = make_blob(std::move(decompressed));
    // Recompute the digest the same way callers produce it (over a blob, whose
    // hash carries a type tag), so an uncorrupted value verifies correctly.
    if (store_->check_file_data())
    {
        if (digest_of(result) != entry->digest)
        {
            throw disk_cas_error("digest mismatch on decompressed data");
        }
    }
    co_return result;
}

cppcoro::task<bool>
disk_cas_impl::exists(digest key)
{
    co_await store_->read_pool().schedule();
    co_return store_->cache().look_up_ac_id(cas_ac_key(key)).has_value();
}

std::unique_ptr<cas_intf>
make_disk_cas(std::shared_ptr<local_durable_store> store, std::string name)
{
    return std::make_unique<disk_cas_impl>(std::move(store), std::move(name));
}

} // namespace cradle
