#include <iomanip>
#include <sstream>

#include <cradle/inner/introspection/tasklet_impl.h>
#include <cradle/inner/introspection/tasklet_info.h>

namespace cradle {

std::string
to_string(tasklet_event_type what)
{
    static const char* const strings[] = {
        "scheduled",
        "running",
        "before co_await",
        "after co_await",
        "finished",
    };
    int index = static_cast<int>(what);
    return std::string{strings[index]};
}

tasklet_event::tasklet_event(tasklet_event_type what) : tasklet_event{what, ""}
{
}

tasklet_event::tasklet_event(
    tasklet_event_type what, std::string const& details)
    : when_{std::chrono::system_clock::now()}, what_{what}, details_{details}
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

std::vector<tasklet_info>
get_tasklet_infos(bool include_finished)
{
    return tasklet_admin::instance().get_tasklet_infos(include_finished);
}

void
introspection_set_capturing_enabled(bool enabled)
{
    return tasklet_admin::instance().set_capturing_enabled(enabled);
}

void
introspection_set_logging_enabled(bool enabled)
{
    return tasklet_admin::instance().set_logging_enabled(enabled);
}

void
introspection_clear_info()
{
    return tasklet_admin::instance().clear_info();
}

} // namespace cradle
