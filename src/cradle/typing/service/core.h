#ifndef CRADLE_TYPING_SERVICE_CORE_H
#define CRADLE_TYPING_SERVICE_CORE_H

#include <memory>

#include <cppcoro/fmap.hpp>

#include <cradle/inner/service/core.h>
#include <cradle/typing/io/http_requests.hpp>
#include <cradle/typing/service/internals.h>
#include <cradle/typing/service/types.hpp>

namespace cradle {

namespace detail {

struct service_core_internals;

}

struct service_core : public inner_service_core
{
    service_core() : inner_service_core()
    {
    }
    service_core(service_config const& config) : inner_service_core()
    {
        reset(config);
    }
    ~service_core();

    void
    reset();
    void
    reset(service_config const& config);

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

template<>
cppcoro::task<dynamic>
disk_cached(
    inner_service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<dynamic>()> create_task);

template<class Value>
cppcoro::task<Value>
disk_cached(
    inner_service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<Value>()> create_task)
{
    return cppcoro::make_task(cppcoro::fmap(
        CRADLE_LAMBDIFY(from_dynamic<Value>),
        disk_cached<dynamic>(
            core, key, [create_task = std::move(create_task)]() {
                return cppcoro::make_task(cppcoro::fmap(
                    CRADLE_LAMBDIFY(to_dynamic<Value>), create_task()));
            })));
}

// Initialize a service for unit testing purposes.
void
init_test_service(service_core& core);

// Set up HTTP mocking for a service.
// This returns the mock_http_session that's been associated with the service.
struct mock_http_session;
mock_http_session&
enable_http_mocking(service_core& core);

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
