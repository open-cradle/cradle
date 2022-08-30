#include <cradle/typing/service/core.h>

#include <thread>

#include <boost/lexical_cast.hpp>

#include <spdlog/spdlog.h>

#include <cppcoro/schedule_on.hpp>

#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/typing/encodings/native.h>
#include <cradle/typing/service/internals.h>

namespace cradle {

namespace {

inner_service_config
make_inner_service_config(service_config const& svc_config)
{
    inner_service_config res;
    if (svc_config.immutable_cache)
    {
        res.immutable_cache = immutable_cache_config{static_cast<size_t>(
            svc_config.immutable_cache->unused_size_limit)};
    }
    if (svc_config.disk_cache)
    {
        res.disk_cache = disk_cache_config{
            svc_config.disk_cache->directory,
            static_cast<size_t>(svc_config.disk_cache->size_limit)};
    }
    return res;
}

} // namespace

void
service_core::reset()
{
    inner_reset();
    impl_.reset();
}

void
service_core::reset(service_config const& svc_config)
{
    inner_reset(make_inner_service_config(svc_config));
    impl_.reset(new detail::service_core_internals{
        .http_pool = cppcoro::static_thread_pool(
            svc_config.http_concurrency ? *svc_config.http_concurrency : 36),
        .local_compute_pool{},
        .mock_http{}});
}

service_core::~service_core()
{
}

http_connection_interface&
http_connection_for_thread(service_core& core)
{
    if (core.internals().mock_http)
    {
        thread_local mock_http_connection the_connection(
            *core.internals().mock_http);
        return the_connection;
    }
    else
    {
        static http_request_system the_system;
        thread_local http_connection the_connection(the_system);
        return the_connection;
    }
}

cppcoro::task<http_response>
internal_async_http_request(
    service_core& core, http_request request, tasklet_tracker* tasklet)
{
    tasklet_run tasklet_run(tasklet);
    null_check_in check_in;
    null_progress_reporter reporter;
    co_return http_connection_for_thread(core).perform_request(
        check_in, reporter, request);
}

cppcoro::task<http_response>
async_http_request(
    service_core& core, http_request request, tasklet_tracker* client)
{
    std::ostringstream s;
    s << "HTTP: " << request.method << " " << request.url;
    auto tasklet = create_tasklet_tracker("HTTP", s.str(), client);
    co_return co_await schedule_on(
        core.internals().http_pool,
        internal_async_http_request(core, std::move(request), tasklet));
}

// This is a coroutine so takes key by value.
template<>
cppcoro::task<dynamic>
disk_cached(
    inner_service_core& core,
    captured_id key,
    std::function<cppcoro::task<dynamic>()> create_task)
{
    auto dynamic_to_blob = [](dynamic x) -> blob {
        return make_blob(write_natively_encoded_value(x));
    };
    auto create_blob_task = [&]() {
        return cppcoro::make_task(
            cppcoro::fmap(dynamic_to_blob, create_task()));
    };
    blob x = co_await disk_cached<blob>(core, key, create_blob_task);
    auto data = reinterpret_cast<uint8_t const*>(x.data());
    co_return read_natively_encoded_value(data, x.size());
}

void
init_test_service(service_core& core)
{
    auto cache_dir = file_path("service_disk_cache");

    reset_directory(cache_dir);

    core.reset(service_config(
        service_immutable_cache_config(0x40'00'00'00),
        service_disk_cache_config(some(cache_dir.string()), 0x40'00'00'00),
        2,
        2,
        2));
}

mock_http_session&
enable_http_mocking(service_core& core)
{
    core.internals().mock_http = std::make_unique<mock_http_session>();
    return *core.internals().mock_http;
}

} // namespace cradle
