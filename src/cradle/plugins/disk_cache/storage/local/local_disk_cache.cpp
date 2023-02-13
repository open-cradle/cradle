// A reference key-value store based on a local disk cache.

#include <cppcoro/fmap.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <spdlog/spdlog.h>

// Boost.Crc triggers some warnings on MSVC.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4245)
#pragma warning(disable : 4701)
#include <boost/crc.hpp>
#pragma warning(pop)
#else
#include <boost/crc.hpp>
#endif

#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/base64.h>
#include <cradle/inner/encodings/lz4.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/service/disk_cache_intf.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache.h>

namespace cradle {

static cppcoro::task<std::string>
read_file_contents(
    cppcoro::static_thread_pool& read_pool, file_path const& path)
{
    co_await read_pool.schedule();
    co_return read_file_contents(path);
}

static std::string
blob_to_string(blob const& x)
{
    std::ostringstream os;
    os << "<blob - size: " << x.size() << " bytes>";
    return os.str();
}

static struct ll_disk_cache_config
make_ll_disk_cache_config(service_config const& config)
{
    return ll_disk_cache_config{
        config.get_optional_string(local_disk_cache_config_keys::DIRECTORY),
        config.get_optional_number(local_disk_cache_config_keys::SIZE_LIMIT)};
}

static uint32_t
get_num_threads_read_pool(service_config const& config)
{
    return static_cast<uint32_t>(config.get_number_or_default(
        local_disk_cache_config_keys::NUM_THREADS_READ_POOL, 2));
}

static unsigned
get_num_threads_write_pool(service_config const& config)
{
    return static_cast<unsigned>(config.get_number_or_default(
        local_disk_cache_config_keys::NUM_THREADS_WRITE_POOL, 2));
}

local_disk_cache::local_disk_cache(service_config const& config)
    : ll_cache_{make_ll_disk_cache_config(config)},
      read_pool_{get_num_threads_read_pool(config)},
      write_pool_{get_num_threads_write_pool(config)}
{
}

void
local_disk_cache::reset(service_config const& config)
{
    ll_cache_.reset(make_ll_disk_cache_config(config));
}

// This is a coroutine so takes id_key by value.
cppcoro::task<blob>
local_disk_cache::disk_cached_blob(
    captured_id id_key, std::function<cppcoro::task<blob>()> create_task)
{
    std::string key{get_unique_string(*id_key)};
    // Check the cache for an existing value.
    try
    {
        auto entry = ll_cache_.find(key);
        if (entry)
        {
            spdlog::get("cradle")->info("disk cache hit on {}", key);

            if (entry->value)
            {
                blob x{base64_decode(
                    *entry->value, get_mime_base64_character_set())};
                spdlog::get("cradle")->debug(
                    "deserialized: {}", blob_to_string(x));
                co_return x;
            }
            else
            {
                spdlog::get("cradle")->debug("reading file", key);
                auto data = co_await read_file_contents(
                    read_pool_, ll_cache_.get_path_for_id(entry->id));

                spdlog::get("cradle")->debug("decompressing", key);
                auto original_size
                    = boost::numeric_cast<size_t>(entry->original_size);
                byte_vector decompressed(original_size);
                lz4::decompress(
                    decompressed.data(),
                    original_size,
                    data.data(),
                    data.size());

                spdlog::get("cradle")->debug("checking CRC", key);
                boost::crc_32_type crc;
                crc.process_bytes(decompressed.data(), original_size);
                if (crc.checksum() == entry->crc32)
                {
                    spdlog::get("cradle")->debug("returning", key);
                    co_return make_blob(std::move(decompressed));
                }
            }
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error reading disk cache entry {}", key);
    }
    spdlog::get("cradle")->debug("disk cache miss on {}", key);

    // We didn't get it from the cache, so actually create the task to compute
    // the result.
    auto result = co_await create_task();

    // Cache the result.
    write_pool_.push_task([&ll_cache = ll_cache_, key, result] {
        try
        {
            if (result.size() > 1024)
            {
                size_t max_compressed_size
                    = lz4::max_compressed_size(result.size());

                std::unique_ptr<uint8_t[]> compressed_data(
                    new uint8_t[max_compressed_size]);
                size_t actual_compressed_size = lz4::compress(
                    compressed_data.get(),
                    max_compressed_size,
                    result.data(),
                    result.size());

                auto cache_id = ll_cache.initiate_insert(key);
                {
                    auto entry_path = ll_cache.get_path_for_id(cache_id);
                    std::ofstream output;
                    open_file(
                        output,
                        entry_path,
                        std::ios::out | std::ios::trunc | std::ios::binary);
                    output.write(
                        reinterpret_cast<char const*>(compressed_data.get()),
                        actual_compressed_size);
                }
                boost::crc_32_type crc;
                crc.process_bytes(result.data(), result.size());
                ll_cache.finish_insert(
                    cache_id, crc.checksum(), result.size());
            }
            else
            {
                ll_cache.insert(
                    key,
                    base64_encode(
                        reinterpret_cast<uint8_t const*>(result.data()),
                        result.size(),
                        get_mime_base64_character_set()));
            }
        }
        catch (std::exception& e)
        {
            // Something went wrong trying to write the cached value, so issue
            // a warning and move on.
            spdlog::get("cradle")->warn(
                "error writing disk cache entry {}", key);
            spdlog::get("cradle")->warn(e.what());
        }
    });

    co_return result;
}

} // namespace cradle
