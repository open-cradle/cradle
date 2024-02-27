#include <iomanip>
#include <sstream>

#include <cradle/inner/introspection/tasklet_impl.h>
#include <cradle/inner/introspection/tasklet_info.h>

namespace cradle {

namespace {

static std::string const event_type_strings[] = {
    "scheduled",
    "running",
    "before co_await",
    "after co_await",
    "finished",
    "unknown",
};

static constexpr int num_event_type_strings
    = sizeof(event_type_strings) / sizeof(event_type_strings[0]);

} // namespace

std::string
to_string(tasklet_event_type what)
{
    int index = static_cast<int>(what);
    return event_type_strings[index];
}

tasklet_event_type
to_tasklet_event_type(std::string const& what_string)
{
    for (int i = 0; i < num_event_type_strings; ++i)
    {
        if (what_string == event_type_strings[i])
        {
            return tasklet_event_type{i};
        }
    }
    return tasklet_event_type::UNKNOWN;
}

tasklet_event::tasklet_event(tasklet_event_type what) : tasklet_event{what, ""}
{
}

tasklet_event::tasklet_event(
    tasklet_event_type what, std::string const& details)
    : when_{std::chrono::system_clock::now()}, what_{what}, details_{details}
{
}

tasklet_event::tasklet_event(
    std::chrono::time_point<std::chrono::system_clock> when,
    tasklet_event_type what,
    std::string const& details)
    : when_{when}, what_{what}, details_{details}
{
}

tasklet_info::tasklet_info(tasklet_impl const& impl)
    : own_id_{impl.own_id()},
      pool_name_{impl.pool_name()},
      title_{impl.title()},
      client_id_{impl.client() ? impl.client()->own_id() : NO_TASKLET_ID}
{
    for (auto const& opt_evt : impl.optional_events())
    {
        if (opt_evt)
        {
            events_.push_back(*opt_evt);
        }
    }
}

tasklet_info::tasklet_info(
    int own_id,
    std::string pool_name,
    std::string title,
    int client_id,
    std::vector<tasklet_event> events)
    : own_id_{own_id},
      pool_name_{std::move(pool_name)},
      title_{std::move(title)},
      client_id_{client_id},
      events_{std::move(events)}
{
}

tasklet_info_list
get_tasklet_infos(tasklet_admin& admin, bool include_finished)
{
    return admin.get_tasklet_infos(include_finished);
}

void
introspection_set_capturing_enabled(tasklet_admin& admin, bool enabled)
{
    admin.set_capturing_enabled(enabled);
}

void
introspection_set_logging_enabled(tasklet_admin& admin, bool enabled)
{
    admin.set_logging_enabled(enabled);
}

void
introspection_clear_info(tasklet_admin& admin)
{
    admin.clear_info();
}

} // namespace cradle
