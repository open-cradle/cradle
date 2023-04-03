#ifndef CRADLE_INNER_SERVICE_RESOURCES_H
#define CRADLE_INNER_SERVICE_RESOURCES_H

// Resources available for resolving requests: the memory cache, and optionally
// some secondary cache (e.g., a disk cache).

#include <memory>
#include <optional>

#include <cppcoro/static_thread_pool.hpp>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/blob_file/blob_file_dir.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/caching/secondary_cache_intf.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/service/config.h>

namespace cradle {

class inner_resources;
class inner_resources_impl;

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

    // (Optional integer)
    // How many concurrent threads to use for HTTP requests
    inline static std::string const HTTP_CONCURRENCY{"http_concurrency"};

    // (Optional integer)
    // How many concurrent threads to use for locally resolving asynchronous
    // requests in parallel
    inline static std::string const ASYNC_CONCURRENCY{"async_concurrency"};
};

// Factory of secondary_cache_intf objects.
// A "disk cache" type of plugin would implement one such factory.
class secondary_cache_factory
{
 public:
    virtual ~secondary_cache_factory() = default;

    virtual std::unique_ptr<secondary_cache_intf>
    create(inner_resources& resources, service_config const& config) = 0;
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
    inner_resources();

    virtual ~inner_resources();

    void
    inner_initialize(service_config const& config);

    void
    reset_memory_cache();

    void
    reset_memory_cache(service_config const& config);

    void
    reset_secondary_cache(service_config const& config);

    cradle::immutable_cache&
    memory_cache();

    secondary_cache_intf&
    secondary_cache();

    std::shared_ptr<blob_file_writer>
    make_blob_file_writer(std::size_t size);

    void
    ensure_async_db();

    // Returns pointer to async_db instance if available (i.e., on an
    // RPC server), nullptr otherwise
    async_db*
    get_async_db();

    cppcoro::static_thread_pool&
    get_async_thread_pool();

    inner_resources_impl&
    impl()
    {
        return *impl_;
    }

 private:
    std::unique_ptr<inner_resources_impl> impl_;
};

http_connection_interface&
http_connection_for_thread(inner_resources& resources);

cppcoro::task<http_response>
async_http_request(
    inner_resources& resources,
    http_request request,
    tasklet_tracker* client = nullptr);

// Initialize a service for unit testing purposes.
void
init_test_service(inner_resources& resources);

// Set up HTTP mocking for a service.
// This returns the mock_http_session that's been associated with the service.
struct mock_http_session;
mock_http_session&
enable_http_mocking(
    inner_resources& resources, bool http_is_synchronous = false);

} // namespace cradle

#endif
