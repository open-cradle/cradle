#ifndef CRADLE_THINKNODE_CACHING_H
#define CRADLE_THINKNODE_CACHING_H

#include <optional>

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/caching/immutable.h>
#include <cradle/inner/caching/immutable/cache.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/service/resources.h>
#include <cradle/thinknode/secondary_cached.h>

namespace cradle {

// This is legacy code, not using the requests architecture.

// This function is a coroutine so takes `key` by value.
// The `cache` lifetime is assumed to exceed the coroutine's one.
template<class Value>
cppcoro::shared_task<Value>
cache_task_wrapper(
    detail::immutable_cache_impl& cache,
    captured_id key,
    cppcoro::task<Value> task)
{
    try
    {
        Value value = co_await task;
        record_immutable_cache_value(cache, *key, deep_sizeof(value));
        co_return value;
    }
    catch (...)
    {
        record_immutable_cache_failure(cache, *key);
        throw;
    }
}

template<class Value, class CreateTask>
auto
wrap_task_creator(CreateTask&& create_task)
{
    return [create_task = std::forward<CreateTask>(create_task)](
               detail::immutable_cache_impl& cache, captured_id const& key) {
        return cache_task_wrapper<Value>(cache, key, create_task(key));
    };
}

template<class Value, class CreateTask>
cppcoro::shared_task<Value>
cached(
    inner_resources& resources,
    captured_id const& key,
    CreateTask&& create_task)
{
    immutable_cache_ptr<Value> ptr(
        resources.memory_cache(),
        key,
        wrap_task_creator<Value>(std::forward<CreateTask>(create_task)));
    return ptr.task();
}

template<class Value, class TaskCreator>
cppcoro::shared_task<Value>
fully_cached(
    inner_resources& resources,
    captured_id const& key,
    TaskCreator task_creator)
{
    // cached() will ensure that a captured id_interface object exists
    // equalling `key`; it will pass a reference to that object to the lambda.
    // With the current implementation, `key` and `key1` refer to the same
    // id_interface object, but this has not always been so.
    return cached<Value>(
        resources, key, [&resources, task_creator](captured_id const& key1) {
            return secondary_cached<Value>(
                resources, key1, std::move(task_creator));
        });
}

namespace detail {

/*
 * Coroutine co_await'ing a shared task and tracking this
 *
 * cache_key must be available after the initial suspension point, so ownership
 * must be inside this function.
 */
template<typename Value>
cppcoro::shared_task<Value>
shared_task_wrapper(
    cppcoro::shared_task<Value> shared_task,
    tasklet_tracker* client,
    captured_id cache_key,
    std::string summary)
{
    // TODO on_before_await() is called when this coroutine is created, not
    // when it is being co_await'ed on
    client->on_before_await(summary, *cache_key);
    auto res = co_await shared_task;
    client->on_after_await();
    co_return res;
}

} // namespace detail

/**
 * Makes a shared task producing some cacheable object, on behalf of a tasklet
 * client
 *
 * - Is or wraps a cppcoro::shared_task<Value> object.
 * - The cacheable object is identified by a captured_id.
 * - client will be nullptr while introspection is disabled.
 *
 * This construct has to be used when needing to co_await on a coroutine that
 * calculates the cache key. If co_await and key calculation are co-located, a
 * direct tasklet_await is also possible. (Both options are currently in use.)
 */
template<typename Value, typename TaskCreator>
cppcoro::shared_task<Value>
make_shared_task_for_cacheable(
    inner_resources& resources,
    captured_id const& cache_key,
    TaskCreator task_creator,
    tasklet_tracker* client,
    std::string summary)
{
    auto shared_task
        = fully_cached<Value>(resources, cache_key, std::move(task_creator));
    if (client)
    {
        return detail::shared_task_wrapper<Value>(
            std::move(shared_task), client, cache_key, std::move(summary));
    }
    else
    {
        return shared_task;
    }
}

} // namespace cradle

#endif
