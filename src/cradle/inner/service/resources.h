#ifndef CRADLE_INNER_SERVICE_RESOURCES_H
#define CRADLE_INNER_SERVICE_RESOURCES_H

// Resources available for resolving requests, e.g. memory and secondary cache

#include <memory>
#include <optional>

#include <cppcoro/static_thread_pool.hpp>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/blob_file/blob_file_dir.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/requests/domain.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/secondary_storage_intf.h>

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
    // register_secondary_storage_factory().
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

// Factory of secondary_storage_intf objects.
// A "disk cache" type of plugin would implement one such factory.
class secondary_storage_factory
{
 public:
    virtual ~secondary_storage_factory() = default;

    virtual std::unique_ptr<secondary_storage_intf>
    create(inner_resources& resources, service_config const& config) = 0;
};

// The secondary storage factories registered for an inner_resources instance.
class secondary_storage_factory_registry
{
 public:
    secondary_storage_factory_registry(inner_resources& resources);

    void
    register_factory(
        std::string const& key,
        std::unique_ptr<secondary_storage_factory> factory);

    std::unique_ptr<secondary_storage_intf>
    create(service_config const& config);

 private:
    inner_resources& resources_;
    std::unordered_map<std::string, std::unique_ptr<secondary_storage_factory>>
        factories_;
};

class inner_resources
{
 public:
    // Creates an object that needs an inner_initialize() call
    inner_resources();

    virtual ~inner_resources();

    // Registers a secondary cache factory, identified by a key.
    // Must happen before inner_initialize().
    void
    register_secondary_storage_factory(
        std::string const& key,
        std::unique_ptr<secondary_storage_factory> factory);

    void
    inner_initialize(service_config const& config);

    void
    reset_memory_cache();

    void
    reset_memory_cache(service_config const& config);

    void
    clear_secondary_cache();

    cradle::immutable_cache&
    memory_cache();

    secondary_storage_intf&
    secondary_cache();

    std::shared_ptr<blob_file_writer>
    make_blob_file_writer(std::size_t size);

    // Ensures that the async_db instance is available.
    // Thread-safe.
    void
    ensure_async_db();

    // Returns pointer to async_db instance if available (i.e., on an
    // RPC server), nullptr otherwise
    async_db*
    get_async_db();

    cppcoro::static_thread_pool&
    get_async_thread_pool();

    void
    register_domain(std::unique_ptr<domain> dom);

    domain&
    find_domain(std::string const& name);

    void
    register_proxy(std::unique_ptr<remote_proxy> proxy);

    remote_proxy&
    get_proxy(std::string const& name);

    inner_resources_impl&
    impl()
    {
        return *impl_;
    }

 private:
    secondary_storage_factory_registry secondary_storage_factories_;
    std::unique_ptr<inner_resources_impl> impl_;
};

http_connection_interface&
http_connection_for_thread(inner_resources& resources);

cppcoro::task<http_response>
async_http_request(
    inner_resources& resources,
    http_request request,
    tasklet_tracker* client = nullptr);

// Set up HTTP mocking for a service.
// This returns the mock_http_session that's been associated with the service.
struct mock_http_session;
mock_http_session&
enable_http_mocking(
    inner_resources& resources, bool http_is_synchronous = false);

} // namespace cradle

#endif
