#include <chrono>

#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/websocket/introspection.h>

namespace cradle {

namespace {

template<typename TimePoint>
decltype(auto)
to_millis(TimePoint time_point)
{
    auto duration = duration_cast<std::chrono::milliseconds>(
        time_point.time_since_epoch());
    return duration.count();
}

tasklet_msg_event
make_tasklet_msg_event_for_local(tasklet_event const& event)
{
    return make_tasklet_msg_event(
        to_millis(event.when()), to_string(event.what()), event.details());
}

tasklet_overview
make_tasklet_overview_for_local(tasklet_info const& info)
{
    omissible<integer> client_id;
    if (info.have_client())
    {
        client_id = info.client_id();
    }
    std::vector<tasklet_msg_event> msg_events;
    for (auto const& e : info.events())
    {
        msg_events.push_back(make_tasklet_msg_event_for_local(e));
    }
    return make_tasklet_overview(
        info.pool_name(),
        info.own_id(),
        client_id,
        info.title(),
        std::move(msg_events));
}

tasklet_info_tuple_list
get_tasklet_infos_from_remote(remote_proxy& proxy, bool include_finished)
{
    return proxy.get_tasklet_infos(include_finished);
}

tasklet_msg_event
make_tasklet_msg_event_for_remote(tasklet_event_tuple const& event)
{
    return make_tasklet_msg_event(
        std::get<0>(event), std::get<1>(event), std::get<2>(event));
}

tasklet_overview
make_tasklet_overview_for_remote(tasklet_info_tuple const& info)
{
    auto own_id = std::get<0>(info);
    auto pool_name = std::get<1>(info);
    auto title = std::get<2>(info);
    auto client_id = std::get<3>(info);
    omissible<integer> omissible_client_id;
    if (client_id != NO_TASKLET_ID)
    {
        omissible_client_id = client_id;
    }
    auto events = std::get<4>(info);
    std::vector<tasklet_msg_event> msg_events;
    for (auto const& e : events)
    {
        msg_events.push_back(make_tasklet_msg_event_for_remote(e));
    }
    return make_tasklet_overview(
        pool_name, own_id, omissible_client_id, title, std::move(msg_events));
}

} // namespace

introspection_status_response
make_introspection_status_response(remote_proxy& proxy, bool include_finished)
{
    std::vector<tasklet_machine_overview> machines;
    std::vector<tasklet_overview> local_overviews;
    for (auto t : get_tasklet_infos(include_finished))
    {
        local_overviews.push_back(make_tasklet_overview_for_local(t));
    }
    machines.push_back(
        make_tasklet_machine_overview("local", std::move(local_overviews)));
    std::vector<tasklet_overview> rpclib_overviews;
    for (auto t : get_tasklet_infos_from_remote(proxy, include_finished))
    {
        rpclib_overviews.push_back(make_tasklet_overview_for_remote(t));
    }
    machines.push_back(
        make_tasklet_machine_overview("rpclib", std::move(rpclib_overviews)));
    auto now = std::chrono::system_clock::now();
    return make_introspection_status_response(
        to_millis(now), std::move(machines));
}

void
introspection_control(cradle::introspection_control_request const& request)
{
    switch (get_tag(request))
    {
        case introspection_control_request_tag::ENABLED: {
            bool enabled = as_enabled(request);
            introspection_set_capturing_enabled(enabled);
            introspection_set_logging_enabled(enabled);
            break;
        }
        case introspection_control_request_tag::CLEAR_ADMIN:
            introspection_clear_info();
            break;
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("introspection_control_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
            break;
    }
}

} // namespace cradle
