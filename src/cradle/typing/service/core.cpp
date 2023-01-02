#include <cradle/typing/service/core.h>

#include <thread>

#include <boost/lexical_cast.hpp>

#include <spdlog/spdlog.h>

#include <cppcoro/schedule_on.hpp>

#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/typing/service/internals.h>

namespace cradle {

void
service_core::initialize(service_config const& config)
{
    inner_initialize(config);
    impl_.reset(new detail::service_core_internals{
        .http_pool = cppcoro::static_thread_pool(config.get_number_or_default(
            typing_config_keys::HTTP_CONCURRENCY, 36)),
        .local_compute_pool{},
        .mock_http{}});
}

http_connection_interface&
http_connection_for_thread(service_core& core)
{
    if (core.internals().mock_http)
    {
        if (core.internals().http_is_synchronous)
        {
            return core.internals().mock_http->synchronous_connection();
        }
        else
        {
            thread_local mock_http_connection the_connection(
                *core.internals().mock_http);
            return the_connection;
        }
    }
    else
    {
        static http_request_system the_system;
        thread_local http_connection the_connection(the_system);
        return the_connection;
    }
}

cppcoro::task<http_response>
async_http_request(
    service_core& core, http_request request, tasklet_tracker* client)
{
    std::ostringstream s;
    s << "HTTP: " << request.method << " " << request.url;
    auto tasklet = create_tasklet_tracker("HTTP", s.str(), client);
    if (!core.internals().http_is_synchronous)
    {
        co_await core.internals().http_pool.schedule();
    }
    tasklet_run tasklet_run(tasklet);
    null_check_in check_in;
    null_progress_reporter reporter;
    co_return http_connection_for_thread(core).perform_request(
        check_in, reporter, request);
}

mock_http_session&
enable_http_mocking(service_core& core, bool http_is_synchronous)
{
    if (!core.internals().mock_http)
    {
        core.internals().mock_http = std::make_unique<mock_http_session>();
        core.internals().http_is_synchronous = http_is_synchronous;
    }
    return *core.internals().mock_http;
}

} // namespace cradle
