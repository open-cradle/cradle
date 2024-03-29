#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/introspection/tasklet_impl.h>
#include <cradle/inner/introspection/tasklet_info.h>

namespace cradle {

int tasklet_impl::next_id = 1;

// Called only from tasklet_admin::new_tasklet(),
// protected by its lock.
tasklet_impl::tasklet_impl(
    bool logging_enabled,
    std::string const& pool_name,
    std::string const& title,
    tasklet_impl* client)
    : id_{next_id++},
      is_placeholder_{false},
      logging_enabled_{logging_enabled},
      pool_name_{pool_name},
      title_{title},
      client_{client},
      finished_{false}
{
    add_event(tasklet_event_type::SCHEDULED);
    std::ostringstream s;
    s << "scheduled (" << title_ << ") on pool " << pool_name;
    if (client)
    {
        s << ", on behalf of " << client->own_id();
    }
    log(s.str());
}

tasklet_impl::tasklet_impl(bool logging_enabled, int rpc_client_id)
    : id_{-rpc_client_id},
      is_placeholder_{true},
      logging_enabled_{logging_enabled},
      pool_name_{"client"},
      title_{"client"},
      client_{nullptr},
      finished_{true}
{
    log(fmt::format("client {}", rpc_client_id));
}

tasklet_impl::~tasklet_impl()
{
    assert(finished_);
    log("destructor");
}

void
tasklet_impl::on_running()
{
    assert(!finished_);
    std::scoped_lock lock{mutex_};
    log("running");
    add_event(tasklet_event_type::RUNNING);
}

void
tasklet_impl::on_finished()
{
    assert(!finished_);
    std::scoped_lock lock{mutex_};
    finished_ = true;
    log("finished");
    add_event(tasklet_event_type::FINISHED);
}

void
tasklet_impl::on_before_await(
    std::string const& msg, id_interface const& cache_key)
{
    assert(!finished_);
    std::scoped_lock lock{mutex_};
    std::ostringstream s;
    s << msg << " " << cache_key.hash();
    log("before co_await " + s.str());
    add_event(tasklet_event_type::BEFORE_CO_AWAIT, s.str());
    remove_event(tasklet_event_type::AFTER_CO_AWAIT);
}

void
tasklet_impl::on_after_await()
{
    assert(!finished_);
    std::scoped_lock lock{mutex_};
    log("after co_await");
    add_event(tasklet_event_type::AFTER_CO_AWAIT);
}

void
tasklet_impl::add_event(tasklet_event_type what)
{
    int index{static_cast<int>(what)};
    events_[index].emplace(what);
}

void
tasklet_impl::add_event(tasklet_event_type what, std::string const& details)
{
    int index{static_cast<int>(what)};
    events_[index].emplace(what, details);
}

void
tasklet_impl::remove_event(tasklet_event_type what)
{
    int index{static_cast<int>(what)};
    events_[index].reset();
}

void
tasklet_impl::log(std::string const& msg)
{
    log(msg.c_str());
}

void
tasklet_impl::log(char const* msg)
{
    if (logging_enabled_)
    {
        std::ostringstream s;
        s << "TASK " << id_ << " " << msg;
        spdlog::get("cradle")->info(s.str());
    }
}

tasklet_admin::tasklet_admin(bool force_finish)
    : capturing_enabled_{false},
      logging_enabled_{false},
      force_finish_{force_finish}
{
}

tasklet_admin::~tasklet_admin()
{
    for (auto it = tasklets_.begin(); it != tasklets_.end();)
    {
        if (force_finish_ && !(*it)->finished())
        {
            (*it)->on_finished();
        }
        if ((*it)->finished())
        {
            delete *it;
            it = tasklets_.erase(it);
        }
        else
        {
            // TODO what to do with detached tasklets?
            ++it;
        }
    }
}

tasklet_tracker*
tasklet_admin::new_tasklet(
    std::string const& pool_name,
    std::string const& title,
    tasklet_tracker* client)
{
    if (capturing_enabled_)
    {
        std::scoped_lock admin_lock{mutex_};
        auto impl_client = static_cast<tasklet_impl*>(client);
        auto tasklet = new tasklet_impl{
            logging_enabled_, pool_name, title, impl_client};
        tasklets_.push_back(tasklet);
        return tasklet;
    }
    else
    {
        return nullptr;
    }
}

tasklet_tracker*
tasklet_admin::new_tasklet(int rpc_client_id)
{
    if (capturing_enabled_)
    {
        std::scoped_lock admin_lock{mutex_};
        auto tasklet = new tasklet_impl{logging_enabled_, rpc_client_id};
        tasklets_.push_back(tasklet);
        return tasklet;
    }
    else
    {
        return nullptr;
    }
}

void
tasklet_admin::set_capturing_enabled(bool enabled)
{
    capturing_enabled_ = enabled;
}

void
tasklet_admin::set_logging_enabled(bool enabled)
{
    logging_enabled_ = enabled;
}

void
tasklet_admin::clear_info()
{
    std::scoped_lock admin_lock{mutex_};
    for (auto it = tasklets_.begin(); it != tasklets_.end();)
    {
        if ((*it)->finished())
        {
            delete *it;
            it = tasklets_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

tasklet_info_list
tasklet_admin::get_tasklet_infos(bool include_finished)
{
    std::scoped_lock admin_lock{mutex_};
    tasklet_info_list res;
    for (auto& t : tasklets_)
    {
        if (!t->is_placeholder() && (include_finished || !t->finished()))
        {
            std::scoped_lock tasklet_lock{t->mutex()};
            res.emplace_back(*t);
        }
    }
    return res;
}

tasklet_tracker*
create_tasklet_tracker(
    tasklet_admin& admin,
    std::string const& pool_name,
    std::string const& title,
    tasklet_tracker* client)
{
    return admin.new_tasklet(pool_name, title, client);
}

tasklet_tracker*
create_tasklet_tracker(tasklet_admin& admin, int rpc_client_id)
{
    return admin.new_tasklet(rpc_client_id);
}

tasklet_tracker*
create_optional_root_tasklet(
    tasklet_admin& admin, std::optional<root_tasklet_spec> opt_spec)
{
    if (!opt_spec)
    {
        return nullptr;
    }
    return create_tasklet_tracker(
        admin, std::move(opt_spec->pool_name), std::move(opt_spec->title));
}

} // namespace cradle
