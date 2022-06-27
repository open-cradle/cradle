#include <cppcoro/fmap.hpp>
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

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/encodings/base64.h>
#include <cradle/inner/encodings/lz4.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/service/core.h>

using std::string;

namespace cradle {

void
inner_service_core::inner_reset()
{
    impl_.reset();
}

void
inner_service_core::inner_reset(inner_service_config const& config)
{
    size_t ic_size{
        config.immutable_cache ? config.immutable_cache->unused_size_limit
                               : 0x40'00'00'00};
    std::optional<string> dc_directory;
    if (config.disk_cache && config.disk_cache->directory)
    {
        dc_directory = *config.disk_cache->directory;
    }
    size_t dc_size{
        config.disk_cache ? config.disk_cache->size_limit : 0x1'00'00'00'00};
    disk_cache_config dc_config{dc_directory, dc_size};
    impl_.reset(new detail::inner_service_core_internals{
        .cache = immutable_cache_config{ic_size},
        .disk_cache = dc_config,
        .disk_read_pool = cppcoro::static_thread_pool(2),
        .disk_write_pool = thread_pool(2)});
}

cppcoro::task<std::string>
read_file_contents(inner_service_core& core, file_path const& path)
{
    co_await core.inner_internals().disk_read_pool.schedule();
    co_return read_file_contents(path);
}

namespace detail {

string
blob_to_string(blob const& x)
{
    std::ostringstream os;
    os << "<blob - size: " << x.size() << " bytes>";
    return os.str();
}

cppcoro::task<blob>
generic_disk_cached(
    inner_service_core& core,
    id_interface const& id_key,
    std::function<cppcoro::task<blob>()> create_task)
{
    unique_hasher hasher;
    id_key.update_hash(hasher);
    std::string key{hasher.get_string()};
    // Check the cache for an existing value.
    auto& cache = core.inner_internals().disk_cache;
    try
    {
        auto entry = cache.find(key);
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
                    core, cache.get_path_for_id(entry->id));

                spdlog::get("cradle")->debug("decompressing", key);
                auto original_size
                    = boost::numeric_cast<size_t>(entry->original_size);
                std::unique_ptr<uint8_t[]> decompressed_data(
                    new uint8_t[original_size]);
                lz4::decompress(
                    decompressed_data.get(),
                    original_size,
                    data.data(),
                    data.size());

                spdlog::get("cradle")->debug("checking CRC", key);
                boost::crc_32_type crc;
                crc.process_bytes(decompressed_data.get(), original_size);
                if (crc.checksum() == entry->crc32)
                {
                    spdlog::get("cradle")->debug("decoding", key);
                    blob decoded{
                        reinterpret_pointer_cast<std::byte const>(
                            std::shared_ptr<uint8_t[]>{
                                std::move(decompressed_data)}),
                        original_size};
                    spdlog::get("cradle")->debug("returning", key);
                    co_return decoded;
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
    core.inner_internals().disk_write_pool.push_task([&core, key, result] {
        auto& cache = core.inner_internals().disk_cache;
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

                auto cache_id = cache.initiate_insert(key);
                {
                    auto entry_path = cache.get_path_for_id(cache_id);
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
                cache.finish_insert(cache_id, crc.checksum(), result.size());
            }
            else
            {
                cache.insert(
                    key,
                    base64_encode(
                        reinterpret_cast<uint8_t const*>(result.data()),
                        result.size(),
                        get_mime_base64_character_set()));
            }
        }
        catch (...)
        {
            // Something went wrong trying to write the cached value, so issue
            // a warning and move on.
            spdlog::get("cradle")->warn(
                "error writing disk cache entry {}", key);
        }
    });

    co_return result;
}

} // namespace detail

// The inner core has just this one specialization.
template<>
cppcoro::task<blob>
disk_cached(
    inner_service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<blob>()> create_task)
{
    return detail::generic_disk_cached(core, key, std::move(create_task));
}

} // namespace cradle
