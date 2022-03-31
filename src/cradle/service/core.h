#ifndef CRADLE_SERVICE_CORE_H
#define CRADLE_SERVICE_CORE_H

#include <memory>

#include <cppcoro/fmap.hpp>
#include <cppcoro/task.hpp>

#include <cradle/caching/immutable.h>
#include <cradle/introspection/tasklet.h>
#include <cradle/io/http_requests.hpp>
#include <cradle/service/internals.h>
#include <cradle/service/types.hpp>

namespace cradle {

namespace detail {

struct service_core_internals;

}

struct service_core
{
    service_core()
    {
    }
    service_core(service_config const& config)
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

template<typename Value>
cppcoro::task<Value>
disk_cached(
    service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<Value>()> create_task);

template<>
cppcoro::task<blob>
disk_cached(
    service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<blob>()> create_task);

template<>
cppcoro::task<dynamic>
disk_cached(
    service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<dynamic>()> create_task);

template<class Value>
cppcoro::task<Value>
disk_cached(
    service_core& core,
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

template<class Value, class TaskCreator>
cppcoro::shared_task<Value>
cached(service_core& core, id_interface const& key, TaskCreator task_creator)
{
    immutable_cache_ptr<Value> ptr(core.internals().cache, key, task_creator);
    return ptr.task();
}

template<class Value, class TaskCreator>
cppcoro::shared_task<Value>
fully_cached(
    service_core& core, id_interface const& key, TaskCreator task_creator)
{
    // cached() will ensure that a captured id_interface object exists
    // equalling `key`; it will pass a reference to that object to the lambda.
    // It will be a different object from `key`; `key` may no longer exist when
    // the lambda is called.
    return cached<Value>(
        core, key, [&core, task_creator](id_interface const& key1) {
            return disk_cached<Value>(core, key1, std::move(task_creator));
        });
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
