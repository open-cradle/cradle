// A reference key-value store based on a local disk cache.

#include <stdexcept>

#include <fmt/format.h>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/lz4.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/secondary_storage_intf.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>

namespace cradle {

class disk_cache_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

static cppcoro::task<std::string>
read_file_contents(
    cppcoro::static_thread_pool& read_pool, file_path const& path)
{
    co_await read_pool.schedule();
    co_return read_file_contents(path);
}

static bool
get_check_file_data(service_config const& config)
{
    return config.get_bool_or_default(
        local_disk_cache_config_keys::CHECK_FILE_DATA, false);
}

static struct ll_disk_cache_config
make_ll_disk_cache_config(service_config const& config)
{
    return ll_disk_cache_config{
        config.get_optional_string(local_disk_cache_config_keys::DIRECTORY),
        config.get_optional_number(local_disk_cache_config_keys::SIZE_LIMIT),
        config.get_bool_or_default(
            local_disk_cache_config_keys::START_EMPTY, false)};
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

static int
get_poll_interval(service_config const& config)
{
    return static_cast<int>(config.get_number_or_default(
        local_disk_cache_config_keys::POLL_INTERVAL, 200));
}

local_disk_cache::local_disk_cache(service_config const& config)
    : check_file_data_{get_check_file_data(config)},
      ll_cache_{make_ll_disk_cache_config(config)},
      poller_{ll_cache_, get_poll_interval(config)},
      read_pool_{get_num_threads_read_pool(config)},
      write_pool_{get_num_threads_write_pool(config)},
      logger_{spdlog::get("cradle")}
{
}

void
local_disk_cache::clear()
{
    ll_cache_.clear();
}

// This is a coroutine so takes key by value.
cppcoro::task<std::optional<blob>>
local_disk_cache::read(std::string key)
{
    try
    {
        auto entry = ll_cache_.find(key);
        if (!entry)
        {
            logger_->info("disk cache miss on {}", key);
            co_return std::nullopt;
        }
        logger_->info("disk cache hit on {}", key);
        if (entry->value)
        {
            logger_->debug(" value: {}", *entry->value);
            co_return *entry->value;
        }
        else
        {
            auto path{ll_cache_.get_path_for_digest(entry->digest)};
            logger_->debug("reading file for key {}: {}", key, path.string());
            auto data = co_await read_file_contents(read_pool_, path);
            auto result = decompress_file_data(key, *entry, data);
            logger_->debug("returning for {}", key);
            co_return result;
        }
    }
    catch (std::exception const& e)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        logger_->error("error reading disk cache file {}: {}", key, e.what());
    }
    co_return std::nullopt;
}

blob
local_disk_cache::decompress_file_data(
    std::string const& key,
    ll_disk_cache_cas_entry const& entry,
    std::string const& data)
{
    logger_->debug("decompressing for {}", key);
    auto original_size = boost::numeric_cast<std::size_t>(entry.original_size);
    byte_vector decompressed(original_size);
    auto decompressed_size = lz4::decompress(
        decompressed.data(), original_size, data.data(), data.size());

    // The file might be corrupt (truncated) if the write operation was
    // interrupted. If so, the decompress operation will most likely fail,
    // and even if it succeeds, the resulting data will be truncated as well.
    // Check this.
    if (decompressed_size != original_size)
    {
        throw disk_cache_error(fmt::format(
            "decompression gave {} bytes, expected {}",
            decompressed_size,
            original_size));
    }

    // Finally, an optional check on the decompressed data's digest; this looks
    // somewhat paranoid and thus is performed only if the configuration says
    // so.
    if (check_file_data_)
    {
        logger_->debug("checking digest over decompressed data");
        auto digest = get_unique_string_tmpl(decompressed);
        if (digest != entry.digest)
        {
            throw disk_cache_error("digest mismatch on decompressed data");
        }
    }

    return make_blob(std::move(decompressed));
}

cppcoro::task<void>
local_disk_cache::write(std::string key, blob value)
{
    write_pool_.detach_task([&ll_cache = ll_cache_,
                             &logger = *logger_,
                             key,
                             value] {
        try
        {
            auto digest{get_unique_string_tmpl(value)};
            if (value.size() > 1024)
            {
                auto optional_cas_id = ll_cache.initiate_insert(key, digest);
                if (optional_cas_id)
                {
                    auto cas_id = *optional_cas_id;
                    auto max_compressed_size
                        = lz4::max_compressed_size(value.size());
                    byte_vector compressed(max_compressed_size);
                    auto actual_compressed_size = lz4::compress(
                        compressed.data(),
                        max_compressed_size,
                        value.data(),
                        value.size());

                    {
                        auto path = ll_cache.get_path_for_digest(digest);
                        logger.debug("writing {}", path.string());
                        std::ofstream output;
                        open_file(
                            output,
                            path,
                            std::ios::out | std::ios::trunc
                                | std::ios::binary);
                        output.write(
                            reinterpret_cast<char const*>(compressed.data()),
                            actual_compressed_size);
                    }
                    ll_cache.finish_insert(
                        cas_id, actual_compressed_size, value.size());
                }
            }
            else
            {
                ll_cache.insert(key, digest, value);
            }
        }
        catch (std::exception& e)
        {
            // Something went wrong trying to write the cached value, so issue
            // a warning and move on.
            logger.warn("error writing disk cache entry {}", key);
            logger.warn(e.what());
        }
    });

    co_return;
}

disk_cache_info
local_disk_cache::get_summary_info()
{
    return ll_cache_.get_summary_info();
}

std::optional<blob>
local_disk_cache::read_raw_value(std::string const& key)
{
    auto opt_entry{ll_cache_.find(key)};
    return opt_entry ? opt_entry->value : std::nullopt;
}

void
local_disk_cache::write_raw_value(std::string const& key, blob const& value)
{
    ll_cache_.insert(key, get_unique_string_tmpl(value), value);
}

bool
local_disk_cache::busy_writing_to_file() const
{
    return write_pool_.get_tasks_total() > 0;
}

} // namespace cradle
