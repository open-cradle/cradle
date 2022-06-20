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

template<class Value, class TaskCreator>
cppcoro::shared_task<Value>
cached(
    inner_service_core& core, captured_id const& key, TaskCreator task_creator)
{
    immutable_cache_ptr<Value> ptr(
        core.inner_internals().cache, key, task_creator);
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
