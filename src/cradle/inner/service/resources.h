#ifndef CRADLE_INNER_SERVICE_RESOURCES_H
#define CRADLE_INNER_SERVICE_RESOURCES_H

// Resources available for resolving requests: the memory cache, and optionally
// some secondary cache (e.g., a disk cache).

#include <memory>
#include <optional>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/blob_file/blob_file_dir.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/caching/secondary_cache_intf.h>
#include <cradle/inner/service/config.h>

namespace cradle {

// Configuration keys for the inner resources
struct inner_config_keys
{
    // (Optional integer)
    // The maximum amount of memory to use for caching results that are no
    // longer in use, in bytes.
    inline static std::string const MEMORY_CACHE_UNUSED_SIZE_LIMIT{
        "memory_cache/unused_size_limit"};

    // (Mandatory string)
    // Specifies the factory to use to create a secondary cache implementation.
    // The string should equal a key passed to
    // register_secondary_cache_factory().
    inline static std::string const SECONDARY_CACHE_FACTORY{
        "secondary_cache/factory"};
};

// Factory of secondary_cache_intf objects.
// A "disk cache" type of plugin would implement one such factory.
class secondary_cache_factory
{
 public:
    virtual ~secondary_cache_factory() = default;

    virtual std::unique_ptr<secondary_cache_intf>
    create(service_config const& config) = 0;
};

// Registers a secondary cache factory, identified by a key.
// A plugin would call this function in its initialization.
void
register_secondary_cache_factory(
    std::string const& key, std::unique_ptr<secondary_cache_factory> factory);

class inner_resources
{
 public:
    // Creates an object that needs an inner_initialize() call
    inner_resources() = default;

    virtual ~inner_resources() = default;

    void
    inner_initialize(service_config const& config);

    void
    inner_reset_memory_cache();

    void
    inner_reset_memory_cache(service_config const& config);

    void
    inner_reset_secondary_cache(service_config const& config);

    cradle::immutable_cache&
    memory_cache()
    {
        return *memory_cache_;
    }

    secondary_cache_intf&
    secondary_cache()
    {
        return *secondary_cache_;
    }

    std::shared_ptr<blob_file_writer>
    make_blob_file_writer(std::size_t size);

 private:
    std::unique_ptr<cradle::immutable_cache> memory_cache_;
    std::unique_ptr<secondary_cache_intf> secondary_cache_;
    std::unique_ptr<blob_file_directory> blob_dir_;

    void
    create_memory_cache(service_config const& config);

    void
    create_secondary_cache(service_config const& config);
};

} // namespace cradle

#endif
