#ifndef CRADLE_INNER_SERVICE_RESOURCES_IMPL_H
#define CRADLE_INNER_SERVICE_RESOURCES_IMPL_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <cppcoro/static_thread_pool.hpp>

#include <cradle/inner/dll/dll_collection.h>
#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/service/config.h>

namespace cradle {

class async_db;
class blob_file_directory;
class domain;
struct immutable_cache;
class inner_resources;
struct mock_http_session;
class remote_proxy;
class secondary_storage_intf;

class inner_resources_impl
{
 public:
    inner_resources_impl(
        inner_resources& wrapper, service_config const& config);

 private:
    friend class inner_resources;

    // Passing request will cause it to be mocked only if mocking is enabled,
    // and the request is not of a "do not mock" class.
    // Requests to a local server should never be mocked.
    http_connection_interface&
    http_connection_for_thread(http_request const* request);

    std::mutex mutex_;
    service_config config_;
    std::unique_ptr<immutable_cache> memory_cache_;
    std::unique_ptr<secondary_storage_intf> secondary_cache_;
    std::unique_ptr<blob_file_directory> blob_dir_;
    std::unique_ptr<async_db> the_async_db_;
    std::unordered_map<std::string, std::unique_ptr<domain>> domains_;
    std::unordered_map<std::string, std::unique_ptr<remote_proxy>> proxies_;
    // the_seri_registry_ is referred to (using shared_ptr's) by
    // seri_catalog's, which could be owned (at least) by domain and
    // dll_collection objects.
    std::shared_ptr<seri_registry> the_seri_registry_;
    dll_collection the_dlls_;

    cppcoro::static_thread_pool http_pool_;
    cppcoro::static_thread_pool async_pool_;

    std::unique_ptr<mock_http_session> mock_http_;

    // Normally, HTTP requests are dispatched to a thread in the HTTP thread
    // pool. Setting this to true causes them to be evaluated on the calling
    // thread. This should happen only for mock HTTP in benchmark tests, where
    // it tends to give more reliable and consistent timings.
    bool http_is_synchronous_{false};
};

} // namespace cradle

#endif
