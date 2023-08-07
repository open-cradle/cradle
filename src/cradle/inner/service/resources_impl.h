#ifndef CRADLE_INNER_SERVICE_RESOURCES_IMPL_H
#define CRADLE_INNER_SERVICE_RESOURCES_IMPL_H

#include <memory>
#include <mutex>
#include <unordered_map>

#include <cppcoro/static_thread_pool.hpp>

#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

class inner_resources_impl
{
 public:
    inner_resources_impl(
        inner_resources& resources, service_config const& config);

    void
    reset_memory_cache();

    void
    reset_memory_cache(service_config const& config);

    void
    reset_secondary_cache(service_config const& config);

    cradle::immutable_cache&
    memory_cache()
    {
        return *memory_cache_;
    }

    secondary_storage_intf&
    secondary_cache()
    {
        return *secondary_cache_;
    }

    std::shared_ptr<blob_file_writer>
    make_blob_file_writer(std::size_t size);

    // Passing request will cause it to be mocked only if mocking is enabled,
    // and the request is not of a "do not mock" class.
    // Requests to a local server should never be mocked.
    http_connection_interface&
    http_connection_for_thread(http_request const* request = nullptr);

    cppcoro::task<http_response>
    async_http_request(http_request request, tasklet_tracker* client);

    mock_http_session&
    enable_http_mocking(bool http_is_synchronous);

    void
    ensure_async_db();

    cppcoro::static_thread_pool&
    get_async_thread_pool()
    {
        return async_pool_;
    }

    // Returns pointer to async_db instance if available (i.e., on an
    // RPC server), nullptr otherwise
    async_db*
    get_async_db();

    void
    register_proxy(std::unique_ptr<remote_proxy> proxy);

    remote_proxy&
    get_proxy(std::string const& name);

 private:
    std::mutex mutex_;
    std::unique_ptr<cradle::immutable_cache> memory_cache_;
    std::unique_ptr<secondary_storage_intf> secondary_cache_;
    std::unique_ptr<blob_file_directory> blob_dir_;
    std::unique_ptr<async_db> async_db_instance_;
    std::unordered_map<std::string, std::unique_ptr<remote_proxy>> proxies_;

    cppcoro::static_thread_pool http_pool_;
    cppcoro::static_thread_pool async_pool_;

    std::unique_ptr<mock_http_session> mock_http_;

    // Normally, HTTP requests are dispatched to a thread in the HTTP thread
    // pool. Setting this to true causes them to be evaluated on the calling
    // thread. This should happen only for mock HTTP in benchmark tests, where
    // it tends to give more reliable and consistent timings.
    bool http_is_synchronous_{false};

    void
    create_memory_cache(service_config const& config);

    void
    create_secondary_cache(
        inner_resources& resources, service_config const& config);
};

} // namespace cradle

#endif
