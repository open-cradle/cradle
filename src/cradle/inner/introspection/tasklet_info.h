#ifndef CRADLE_INNER_INTROSPECTION_TASKLET_INFO_H
#define CRADLE_INNER_INTROSPECTION_TASKLET_INFO_H

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

#include <cradle/inner/introspection/tasklet.h>

namespace cradle {

class tasklet_impl;

/**
 * Types of tasklet lifecycle events
 */
enum class tasklet_event_type
{
    SCHEDULED,
    RUNNING,
    BEFORE_CO_AWAIT,
    AFTER_CO_AWAIT,
    FINISHED,
    UNKNOWN
};
constexpr int num_tasklet_event_types
    = static_cast<int>(tasklet_event_type::UNKNOWN) + 1;

std::string
to_string(tasklet_event_type what);

tasklet_event_type
to_tasklet_event_type(std::string const& what_string);

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

    tasklet_event(
        std::chrono::time_point<std::chrono::system_clock> when,
        tasklet_event_type what,
        std::string const& details);

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

    tasklet_info(
        int own_id,
        std::string pool_name,
        std::string title,
        int client_id,
        std::vector<tasklet_event> events);

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
        return client_id_ != NO_TASKLET_ID;
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

using tasklet_info_list = std::vector<tasklet_info>;

/**
 * Retrieves information on all introspective tasklets
 *
 * This function will be called from a websocket thread that is different from
 * the threads on which the coroutines run, that generate this information. One
 * or more mutexes will be needed.
 */
tasklet_info_list
get_tasklet_infos(tasklet_admin& admin, bool include_finished);

/**
 * Enables or disables capturing of introspection events
 */
void
introspection_set_capturing_enabled(tasklet_admin& admin, bool enabled);

/**
 * Enables or disables introspection logging
 */
void
introspection_set_logging_enabled(tasklet_admin& admin, bool enabled);

/**
 * Clears captured introspection information
 *
 * Objects currently being captured may be excluded from being cleared.
 */
void
introspection_clear_info(tasklet_admin& admin);

} // namespace cradle

#endif
