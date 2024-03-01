#include <chrono>

#include <cradle/inner/introspection/tasklet_util.h>
#include <cradle/rpclib/common/common.h>

namespace cradle {

namespace {

template<typename TimePoint>
static decltype(auto)
to_millis(TimePoint time_point)
{
    auto duration = duration_cast<std::chrono::milliseconds>(
        time_point.time_since_epoch());
    return duration.count();
}

static tasklet_event_tuple
make_event_tuple(tasklet_event const& event)
{
    return tasklet_event_tuple{
        to_millis(event.when()), to_string(event.what()), event.details()};
}

tasklet_info_tuple
make_info_tuple(tasklet_info const& info)
{
    int client_id{NO_TASKLET_ID};
    if (info.have_client())
    {
        client_id = info.client_id();
    }
    std::vector<tasklet_event_tuple> events;
    for (auto const& e : info.events())
    {
        events.push_back(make_event_tuple(e));
    }
    return tasklet_info_tuple{
        info.own_id(),
        info.pool_name(),
        info.title(),
        client_id,
        std::move(events)};
}

} // namespace

tasklet_info_tuple_list
make_info_tuples(tasklet_info_list const& infos)
{
    tasklet_info_tuple_list result;
    for (auto info : infos)
    {
        result.push_back(make_info_tuple(info));
    }
    return result;
}

tasklet_info_list
make_tasklet_infos(tasklet_info_tuple_list const& tuples)
{
    tasklet_info_list infos;
    for (auto const& tup : tuples)
    {
        auto own_id{std::get<0>(tup)};
        auto pool_name{std::get<1>(tup)};
        auto title{std::get<2>(tup)};
        auto client_id{std::get<3>(tup)};
        std::vector<tasklet_event> events;
        auto const& tup_events = std::get<4>(tup);
        for (auto const& tup_event : tup_events)
        {
            std::chrono::milliseconds millis{std::get<0>(tup_event)};
            std::chrono::time_point<std::chrono::system_clock> when{millis};
            auto what = to_tasklet_event_type(std::get<1>(tup_event));
            auto details = std::get<2>(tup_event);
            events.push_back(tasklet_event{when, what, details});
        }
        infos.push_back(tasklet_info{
            own_id,
            std::move(pool_name),
            std::move(title),
            client_id,
            std::move(events)});
    }
    return infos;
}

void
dump_tasklet_infos(tasklet_info_tuple_list const& tuples, std::ostream& os)
{
    dump_tasklet_infos(make_tasklet_infos(tuples), os);
}

} // namespace cradle
