#ifndef CRADLE_TYPING_SERVICE_CORE_H
#define CRADLE_TYPING_SERVICE_CORE_H

// Services exposed by the typing subsystem.

#include <memory>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/typing/io/http_requests.hpp>
#include <cradle/typing/service/internals.h>

namespace cradle {

// Configuration keys for the typing subsystem
struct typing_config_keys
{
    // (Optional integer)
    // How many concurrent threads to use for request handling.
    // The default is one thread for each processor core.
    inline static std::string const REQUEST_CONCURRENCY{"request_concurrency"};

    // (Optional integer)
    // How many concurrent threads to use for computing.
    // The default is one thread for each processor core.
    inline static std::string const COMPUTE_CONCURRENCY{"compute_concurrency"};

    // (Optional integer)
    // How many concurrent threads to use for HTTP requests
    inline static std::string const HTTP_CONCURRENCY{"http_concurrency"};
};

namespace detail {

struct service_core_internals;

}

struct service_core : public inner_resources
{
    service_core() : inner_resources()
    {
    }

    service_core(service_config const& config) : inner_resources()
    {
        initialize(config);
    }

    void
    initialize(service_config const& config);

    detail::service_core_internals&
    internals()
    {
        return *impl_;
    }

 private:
    std::unique_ptr<detail::service_core_internals> impl_;
};

http_connection_interface&
http_connection_for_thread(service_core& core);

cppcoro::task<http_response>
async_http_request(
    service_core& core,
    http_request request,
    tasklet_tracker* client = nullptr);

// Initialize a service for unit testing purposes.
void
init_test_service(service_core& core);

// Set up HTTP mocking for a service.
// This returns the mock_http_session that's been associated with the service.
struct mock_http_session;
mock_http_session&
enable_http_mocking(service_core& core, bool http_is_synchronous = false);

template<class Sequence, class Function>
cppcoro::task<>
for_async(Sequence sequence, Function&& function)
{
    auto i = co_await sequence.begin();
    auto const end = sequence.end();
    while (i != end)
    {
        std::forward<Function>(function)(*i);
        (void) co_await ++i;
    }
}

} // namespace cradle

#endif
