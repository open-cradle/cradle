#ifndef CRADLE_INNER_SERVICE_CORE_H
#define CRADLE_INNER_SERVICE_CORE_H

#include <optional>

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/caching/disk_cache.h>
#include <cradle/inner/caching/immutable.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/service/internals.h>
#include <cradle/inner/service/types.h>

namespace cradle {

template<typename Value>
cppcoro::task<Value>
disk_cached(
    inner_service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<Value>()> create_task);

template<>
cppcoro::task<blob>
disk_cached(
    inner_service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<blob>()> create_task);

template<class Value>
cppcoro::shared_task<Value>
cache_task_wrapper(
    detail::immutable_cache_impl& cache,
    id_interface const& key,
    cppcoro::task<Value> task)
{
    try
    {
        Value value = co_await task;
        record_immutable_cache_value(cache, key, deep_sizeof(value));
        co_return value;
    }
    catch (...)
    {
        record_immutable_cache_failure(cache, key);
        throw;
    }
}

template<class Value, class CreateTask>
auto
wrap_task_creator(CreateTask&& create_task)
{
    return [create_task = std::forward<CreateTask>(create_task)](
               detail::immutable_cache_impl& cache, id_interface const& key) {
        return cache_task_wrapper<Value>(cache, key, create_task(key));
    };
}

template<class Value, class CreateTask>
cppcoro::shared_task<Value>
cached(
    inner_service_core& core, captured_id const& key, CreateTask&& create_task)
{
    immutable_cache_ptr<Value> ptr(
        core.inner_internals().cache,
        key,
        wrap_task_creator<Value>(std::forward<CreateTask>(create_task)));
    return ptr.task();
}

template<class Value, class TaskCreator>
cppcoro::shared_task<Value>
fully_cached(
    inner_service_core& core, captured_id const& key, TaskCreator task_creator)
{
    // cached() will ensure that a captured id_interface object exists
    // equalling `key`; it will pass a reference to that object to the lambda.
    // It may be a different object from `key`: `key` may no longer exist when
    // the lambda is called.
    return cached<Value>(
        core, key, [&core, task_creator](id_interface const& key1) {
            return disk_cached<Value>(core, key1, std::move(task_creator));
        });
}

} // namespace cradle

#endif
