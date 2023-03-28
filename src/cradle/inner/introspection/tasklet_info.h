#ifndef CRADLE_INNER_INTROSPECTION_TASKLET_INFO_H
#define CRADLE_INNER_INTROSPECTION_TASKLET_INFO_H

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

namespace cradle {

class tasklet_impl;
class tasklet_tracker;

/**
 * Types of tasklet lifecycle events
 */
enum class tasklet_event_type
{
    SCHEDULED,
    RUNNING,
    BEFORE_CO_AWAIT,
    AFTER_CO_AWAIT,
    FINISHED
};
constexpr int num_tasklet_event_types
    = static_cast<int>(tasklet_event_type::FINISHED) + 1;

std::string
to_string(tasklet_event_type what);

/**
 * An event in a tasklet's lifecycle
 */
class tasklet_event
{
    std::chrono::time_point<std::chrono::system_clock> when_;
    tasklet_event_type what_;
    std::string details_;

 public:
    explicit tasklet_event(tasklet_event_type what);
    tasklet_event(tasklet_event_type what, std::string const& details);

    std::chrono::time_point<std::chrono::system_clock>
    when() const
    {
        return when_;
    }

    tasklet_event_type
    what() const
    {
        return what_;
    }

    std::string const&
    details() const
    {
        return details_;
    }
};

/**
 * The information that can be retrieved on a tasklet
 */
class tasklet_info
{
    int own_id_;
    std::string pool_name_;
    std::string title_;
    int client_id_;
    std::vector<tasklet_event> events_;

 public:
    tasklet_info(const tasklet_impl& impl);

    int
    own_id() const
    {
        return own_id_;
    }

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

    bool
    have_client() const
    {
        return client_id_ >= 0;
    }

    int
    client_id() const
    {
        assert(have_client());
        return client_id_;
    }

    std::vector<tasklet_event> const&
    events() const
    {
        return events_;
    }
};

/**
 * Retrieves information on all introspective tasklets
 *
 * This function will be called from a websocket thread that is different from
 * the threads on which the coroutines run, that generate this information. One
 * or more mutexes will be needed.
 */
std::vector<tasklet_info>
get_tasklet_infos(bool include_finished);

/**
 * Enables or disables capturing of introspection events
 */
void
introspection_set_capturing_enabled(bool enabled);

/**
 * Enables or disables introspection logging
 */
void
introspection_set_logging_enabled(bool enabled);

/**
 * Clears captured introspection information
 *
 * Objects currently being captured may be excluded from being cleared.
 */
void
introspection_clear_info();

} // namespace cradle

#endif
