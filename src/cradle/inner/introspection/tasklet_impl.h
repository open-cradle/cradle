#ifndef CRADLE_INNER_INTROSPECTION_TASKLET_IMPL_H
#define CRADLE_INNER_INTROSPECTION_TASKLET_IMPL_H

#include <array>
#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_info.h>

namespace cradle {

/**
 * (The only) implementation of the tasklet_tracker interface
 *
 * This object has two roles: recording events in a tasklet's lifetime, and
 * returning information on those events. The first set of functions is called
 * from a coroutine running on some thread, the second set from a different
 * websocket thread. Consequently, data in this object is protected by a mutex.
 * The mutex should be locked for a short time only, leading to a minimal
 * impact on the event-tracking calls.
 *
 * The finished_ variable indicates if the tasklet has finished.
 * It could be accessed from different threads so put inside an atomic.
 */
class tasklet_impl : public tasklet_tracker
{
 public:
    using events_container
        = std::array<std::optional<tasklet_event>, num_tasklet_event_types>;

    // Normal constructor
    tasklet_impl(
        std::string const& pool_name,
        std::string const& title,
        tasklet_impl* client = nullptr);

    // Constructor for a placeholder object on an RPC server, representing the
    // corresponding tasklet on the RPC client
    tasklet_impl(int rpc_client_id);

    ~tasklet_impl();

    bool
    finished() const
    {
        return finished_;
    }

    std::mutex&
    mutex()
    {
        return mutex_;
    }

    int
    own_id() const override
    {
        return id_;
    }

    void
    on_running() override;

    void
    on_finished() override;

    void
    on_before_await(
        std::string const& msg, id_interface const& cache_key) override;

    void
    on_after_await() override;

    void
    log(std::string const& msg) override;

    void
    log(char const* msg) override;

    std::string const&
    pool_name() const
    {
        return pool_name_;
    }

    std::string const&
    title() const
    {
        return title_;
    }

    tasklet_impl const*
    client() const
    {
        return client_;
    }

    events_container const&
    optional_events() const
    {
        return events_;
    }

 private:
    static int next_id;
    int id_;
    std::string pool_name_;
    std::string title_;
    tasklet_impl* client_;
    std::atomic<bool> finished_;
    std::mutex mutex_;
    events_container events_;

    void
    add_event(tasklet_event_type what);

    void
    add_event(tasklet_event_type what, std::string const& details);

    void
    remove_event(tasklet_event_type what);
};

/**
 * Container of all active tasklet_impl objects; singleton
 *
 * Synchronization concerns are similar to the ones for tasklet_impl:
 * - Access to the tasklets_ variable requires locking mutex_.
 * - The capturing_enabled_ boolean is put inside an atomic.
 */
class tasklet_admin
{
    std::atomic<bool> capturing_enabled_;
    std::atomic<bool> logging_enabled_;
    std::mutex mutex_;
    std::list<tasklet_impl*> tasklets_;

    tasklet_admin();

 public:
    /**
     * Returns the singleton
     */
    static tasklet_admin&
    instance();

    /**
     * Creates a new tracker, possibly on behalf of another tasklet (the
     * client)
     */
    tasklet_tracker*
    new_tasklet(
        std::string const& pool_name,
        std::string const& title,
        tasklet_tracker* client = nullptr);

    /**
     * Creates a new tasklet object on an RPC server, corresponding to a
     * tasklet on an RPC client
     */
    tasklet_tracker*
    new_tasklet(int rpc_client_id);

    /**
     * Enables or disables capturing of introspection events
     *
     * While introspection is disabled, it should have no noticeable
     * performance impact.
     */
    void
    set_capturing_enabled(bool enabled);

    void
    set_logging_enabled(bool enabled);

    bool
    logging_enabled() const
    {
        return logging_enabled_;
    }

    /**
     * Deletes the finished tasklet_tracker objects
     */
    void
    clear_info();

    std::vector<tasklet_info>
    get_tasklet_infos(bool include_finished);

    void
    hard_reset_testing_only(bool enabled = true);
};

} // namespace cradle

#endif
