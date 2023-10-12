#ifndef CRADLE_INNER_SERVICE_RESOURCES_H
#define CRADLE_INNER_SERVICE_RESOURCES_H

#include <memory>
#include <optional>

#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/service/config.h>

namespace cradle {

class async_db;
class blob_file_writer;
class dll_collection;
class domain;
struct immutable_cache;
class inner_resources_impl;
class remote_proxy;
class secondary_storage_intf;
class tasklet_tracker;

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

/*
 * A bunch of resources helping to resolve requests:
 * - A memory (immutable) cache
 * - A secondary cache
 * - A blob_file_writer writing blobs in shared memory
 * - An optional async_db instance
 * - A collection of domains
 * - A collection of remote proxies
 * - A collection of loaded DLLs
 * - Thread pools
 * - An optional mock_http_session object
 */
class inner_resources
{
 public:
    // Initially, there is no secondary cache:
    // - Secondary caches are implemented in plugins, that are not accessible
    //   from here.
    // - A plugin may need an inner_resources reference in its constructor
    //   arguments, and creating a plugin from this constructor would mean
    //   passing a reference to an object under construction. It seems better
    //   to solve these (inter-)dependencies outside this class.
    inner_resources(service_config const& config);

    virtual ~inner_resources();

    inner_resources(inner_resources const&) = delete;
    inner_resources&
    operator=(inner_resources const&)
        = delete;
    inner_resources(inner_resources&& other);
    inner_resources&
    operator=(inner_resources&& other);

    service_config const&
    config() const;

    immutable_cache&
    memory_cache();

    void
    reset_memory_cache();

    void
    set_secondary_cache(
        std::unique_ptr<secondary_storage_intf> secondary_cache);

    secondary_storage_intf&
    secondary_cache();

    void
    clear_secondary_cache();

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

    dll_collection&
    the_dlls();

    // For testing purposes only!?
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

// Set up HTTP mocking for a service.
// This returns the mock_http_session that's been associated with the service.
struct mock_http_session;
mock_http_session&
enable_http_mocking(
    inner_resources& resources, bool http_is_synchronous = false);

} // namespace cradle

#endif
