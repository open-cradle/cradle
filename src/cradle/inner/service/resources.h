#ifndef CRADLE_INNER_SERVICE_RESOURCES_H
#define CRADLE_INNER_SERVICE_RESOURCES_H

#include <memory>
#include <optional>

#include <cppcoro/io_service.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/remote/types.h>
#include <cradle/inner/resolve/seri_lock.h>
#include <cradle/inner/service/config.h>

namespace cradle {

class async_db;
class blob_file_writer;
class dll_collection;
class domain;
struct immutable_cache;
class inner_resources_impl;
struct mock_http_session;
class remote_proxy;
class rpclib_client;
class secondary_storage_intf;
class seri_registry;
class tasklet_admin;
class tasklet_tracker;

// Configuration keys for the inner resources
struct inner_config_keys
{
    // (Optional integer)
    // The maximum amount of memory to use for caching results that are no
    // longer in use, in bytes.
    inline static std::string const MEMORY_CACHE_UNUSED_SIZE_LIMIT{
        "memory_cache/unused_size_limit"};

    // (Optional string)
    // Specifies the factory to use to create a secondary cache implementation.
    // The string should equal a key passed to
    // register_secondary_storage_factory().
    // The string will be absent in the configuration of a contained process,
    // and normally be present otherwise.
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
 * A bunch of resources helping to resolve requests.
 *
 * The intention is that the application, or a server, or a test case, creates
 * a single instance of this class early in its initialization, and does not
 * copy or move that instance. This allows other objects, with more limited
 * lifetimes, to hold a reference to an inner_resources object.
 *
 * The resources are:
 * - An optional memory (immutable) cache (present unless in contained mode)
 * - An optional secondary cache
 * - Zero or more requests storages
 * - A blob_file_writer writing blobs in shared memory
 * - An optional async_db instance
 * - A collection of domains
 * - A collection of remote proxies
 * - A collection of loaded DLLs
 * - Thread pools
 * - An optional mock_http_session object
 * - A registry of templates of requests that can be (de-)serialized
 * - A collection of cache record locks that can be released
 * - A collection of introspection tasklets
 * - A pool of rpclib clients for communicating to contained rpclib servers
 *
 * TODO make resources optional? E.g. cacheless resolving doesn't need much.
 * service_config could indicate what to provide.
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
    inner_resources(inner_resources&& other) = delete;
    inner_resources&
    operator=(inner_resources&& other)
        = delete;

    service_config const&
    config() const;

    bool
    support_caching() const;

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

    // Note that a secondary cache and a requests storage can have overlapping
    // keys (identifying requests) but their values will differ, so the two
    // should really be separate.
    void
    set_requests_storage(
        std::unique_ptr<secondary_storage_intf> storage,
        bool is_default = false);

    // Returns the default requests storage.
    // Throws if not set.
    secondary_storage_intf&
    requests_storage();

    // Returns the requests storage identified by name.
    // Throws if not set.
    secondary_storage_intf&
    requests_storage(std::string const& name);

    http_connection_interface&
    http_connection_for_thread();

    cppcoro::task<http_response>
    async_http_request(
        http_request request, tasklet_tracker* client = nullptr);

    // Set up HTTP mocking.
    // This returns the mock_http_session that's been associated with these
    // resources.
    mock_http_session&
    enable_http_mocking(bool http_is_synchronous = false);

    // User code should not call this directly, but through
    // local_context_intf::make_data_owner().
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

    // Supporting CG R.37
    std::shared_ptr<seri_registry>
    get_seri_registry();

    // Allocates an object that can lock a memory cache record.
    // Will be called only on a server; the client will use the record_id
    // member in the returned value to identify the record.
    seri_cache_record_lock_t
    alloc_cache_record_lock();

    // Releases a lock on a memory cache record.
    // record_id must be from a former alloc_cache_record_lock() call.
    void
    release_cache_record_lock(remote_cache_record_id record_id);

    tasklet_admin&
    the_tasklet_admin();

    cppcoro::io_service&
    the_io_service();

    std::unique_ptr<rpclib_client>
    alloc_contained_proxy(std::shared_ptr<spdlog::logger> logger);

    void
    free_contained_proxy(std::unique_ptr<rpclib_client> proxy, bool succeeded);

    // Retrieve the total number of contained calls initiated through these
    // resources (so failed contained calls also count).
    int
    get_num_contained_calls() const;

    void
    increase_num_contained_calls();

 private:
    std::unique_ptr<inner_resources_impl> impl_;
};

// Returns a best-effort reference to a "current resources" object; throws if
// no such object exists.
//
// If more than one inner_resources object is alive, the function may not
// return the intended one.
//
// The function should not be called unless really necessary. This necessity
// exists for deserializing a msgpack-encoded function_request object: a
// seri_registry instance is needed for choosing the correct
// function_request_impl instantiation, but the msgpack library just provides
// the bare serialization data, and provides no hooks for passing any
// additional context information.
inner_resources&
get_current_inner_resources();

} // namespace cradle

#endif
