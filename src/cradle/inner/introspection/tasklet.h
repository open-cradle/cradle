#ifndef CRADLE_INNER_INTROSPECTION_TASKLET_H
#define CRADLE_INNER_INTROSPECTION_TASKLET_H

#include <string>

#include <cppcoro/shared_task.hpp>

#include <cradle/inner/core/id.h>
#include <cradle/inner/generic/generic.h>

namespace cradle {

struct inner_service_core;

/**
 * Tracks a "tasklet": a conceptual task, implemented as a coroutine
 *
 * Its lifecycle:
 * - The coroutine is assigned to a thread pool: the object is created.
 * - The coroutine is resumed on a thread from the pool: on_runnning().
 * - The coroutine goes through several co_await calls:
 *   on_before_await() and on_after_await().
 * - The coroutine ends: on_finished().
 * - The object may live on to track the finished coroutine.
 *
 * The on_...() functions are intended to be called by classes defined below:
 * - on_running() and on_finished() called by a tasklet_run object
 * - on_before_await() and on_after_await() called by a tasklet_await object
 *
 * tasklet_tracker objects are passed around as raw pointers, leading to
 * ownership rules:
 * - It is not possible to delete the object through this interface: ownership
 *   lies elsewhere.
 * - An on_finished() call marks the object as candidate for deletion; no
 *   further calls are allowed on this interface.
 * - The object's owner should not delete it unless on_finished() was called.
 * - There should be an eventual on_finished() call, or a resource leak exists.
 */
class tasklet_tracker
{
 protected:
    virtual ~tasklet_tracker() = default;

 public:
    virtual int
    own_id() const = 0;

    virtual void
    on_running()
        = 0;

    virtual void
    on_finished()
        = 0;

    virtual void
    on_before_await(std::string const& msg, id_interface const& cache_key)
        = 0;

    virtual void
    on_after_await()
        = 0;

    virtual void
    log(std::string const& msg)
        = 0;

    virtual void
    log(char const* msg)
        = 0;
};

/**
 * Start tracking a new tasklet, possibly on behalf of another one (the client)
 *
 * The return value will be nullptr if tracking is disabled.
 */
tasklet_tracker*
create_tasklet_tracker(
    std::string const& pool_name,
    std::string const& title,
    tasklet_tracker* client = nullptr);

/**
 * Tracks the major states of a tasklet (running / finished)
 */
class tasklet_run
{
    tasklet_tracker* tasklet_;

 public:
    tasklet_run(tasklet_tracker* tasklet) : tasklet_(tasklet)
    {
        if (tasklet_)
        {
            tasklet_->on_running();
        }
    }

    ~tasklet_run()
    {
        if (tasklet_)
        {
            tasklet_->on_finished();
        }
    }
};

/**
 * Tracks a tasklet's "co_await fully_cached..." call.
 *
 * Guards the co_await, so this object should be declared just before the
 * co_await, and the point just after the co_await should coincide with the end
 * of this object's lifetime.
 */
class tasklet_await
{
    tasklet_tracker* tasklet_;

 public:
    tasklet_await(
        tasklet_tracker* tasklet,
        std::string const& what,
        id_interface const& cache_key)
        : tasklet_(tasklet)
    {
        if (tasklet_)
        {
            tasklet_->on_before_await(what, cache_key);
        }
    }

    ~tasklet_await()
    {
        if (tasklet_)
        {
            tasklet_->on_after_await();
        }
    }
};

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
    client->on_before_await(summary, *cache_key);
    auto res = co_await shared_task;
    client->on_after_await();
    co_return res;
}

// Preconditions:
// - req must be introspective and cacheable
template<typename Request>
cppcoro::shared_task<typename Request::value_type>
new_shared_task_wrapper(
    cppcoro::shared_task<typename Request::value_type> shared_task,
    std::shared_ptr<Request> req,
    tasklet_tracker& client)
{
    client.on_before_await(req->get_summary(), *req->get_captured_id());
    auto res = co_await shared_task;
    client.on_after_await();
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
    inner_service_core& service,
    captured_id const& cache_key,
    TaskCreator task_creator,
    tasklet_tracker* client,
    std::string summary)
{
    auto shared_task
        = fully_cached<Value>(service, cache_key, std::move(task_creator));
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

template<typename Request>
cppcoro::shared_task<typename Request::value_type>
make_shared_task_for_request(
    inner_service_core& service, // part of request?
    std::shared_ptr<Request> const& shared_req,
    tasklet_tracker* client)
{
    auto shared_task = new_fully_cached<Request>(service, shared_req);
    if constexpr (
        Request::introspective
        && Request::caching_level != caching_level_type::none)
    {
        if (client)
        {
            return detail::new_shared_task_wrapper<Request>(
                std::move(shared_task), shared_req, *client);
        }
    }
    return shared_task;
}

} // namespace cradle

#endif
