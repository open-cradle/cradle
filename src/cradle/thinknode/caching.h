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
#include <cradle/typing/core/unique_hash.h>

namespace cradle {

// This is legacy code, not using the requests architecture.
// TODO Are shared_task's still required for the Thinknode legacy code?

// Like eval_ptr(), but returning a shared_task
template<typename Value>
cppcoro::shared_task<Value>
eval_immutable_cache_ptr(std::unique_ptr<immutable_cache_ptr<Value>> ptr)
{
    co_await ptr->ensure_value_task();
    co_return ptr->get_value();
}

template<class Value, class CreateTask>
cppcoro::shared_task<void>
legacy_resolve_uncached(
    captured_id key, CreateTask create_task, immutable_cache_ptr<Value>& ptr)
{
    try
    {
        ptr.record_value(co_await create_task(key));
    }
    catch (...)
    {
        ptr.record_failure();
        throw;
    }
}

template<class Value, class CreateTask>
cppcoro::shared_task<Value>
cached(inner_resources& resources, captured_id key, CreateTask create_task)
{
    using ptr_type = immutable_cache_ptr<Value>;
    auto ptr{std::make_unique<ptr_type>(
        resources.memory_cache(),
        key,
        [key, create_task = std::move(create_task)](
            untyped_immutable_cache_ptr& ptr) {
            return legacy_resolve_uncached<Value>(
                key, create_task, static_cast<ptr_type&>(ptr));
        })};
    return eval_immutable_cache_ptr(std::move(ptr));
}

template<class Value, class CreateTask>
cppcoro::shared_task<void>
legacy_resolve_secondary_cached(
    inner_resources& resources,
    captured_id key,
    CreateTask create_task,
    immutable_cache_ptr<Value>& ptr)
{
    try
    {
        ptr.record_value(co_await secondary_cached<Value>(
            resources, key, std::move(create_task)));
    }
    catch (...)
    {
        ptr.record_failure();
        throw;
    }
}

template<class Value, class CreateTask>
cppcoro::shared_task<Value>
fully_cached(
    inner_resources& resources, captured_id key, CreateTask create_task)
{
    using ptr_type = immutable_cache_ptr<Value>;
    auto ptr{std::make_unique<ptr_type>(
        resources.memory_cache(),
        key,
        [&resources, key, create_task = std::move(create_task)](
            untyped_immutable_cache_ptr& ptr) {
            return legacy_resolve_secondary_cached<Value>(
                resources, key, create_task, static_cast<ptr_type&>(ptr));
        })};
    return eval_immutable_cache_ptr(std::move(ptr));
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
