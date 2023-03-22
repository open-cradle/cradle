#ifndef CRADLE_INNER_INTROSPECTION_TASKLET_H
#define CRADLE_INNER_INTROSPECTION_TASKLET_H

#include <string>

#include <cradle/inner/core/id.h>

namespace cradle {

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
    own_id() const
        = 0;

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

} // namespace cradle

#endif
