#include <chrono>
#include <ctime>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <cradle/inner/introspection/tasklet_util.h>

namespace cradle {

void
dump_tasklet_infos(tasklet_info_list const& infos, std::ostream& os)
{
    std::ostream_iterator<char> out{os};
    for (int i = 0; i < static_cast<int>(infos.size()); ++i)
    {
        auto const& info = infos[i];
        std::string client_id{"-"};
        if (info.have_client())
        {
            client_id = fmt::format("{}", info.client_id());
        }
        fmt::format_to(
            out,
            "info[{}] own_id {}, pool_name {}, title {}, client_id {}\n",
            i,
            info.own_id(),
            info.pool_name(),
            info.title(),
            client_id);
        auto const& events = info.events();
        for (int j = 0; j < static_cast<int>(events.size()); ++j)
        {
            auto const& event = events[j];
            auto time = std::chrono::system_clock::to_time_t(event.when());
            auto tm = std::localtime(&time);
            auto duration
                = std::chrono::duration_cast<std::chrono::milliseconds>(
                    event.when().time_since_epoch());
            std::string details;
            if (!event.details().empty())
            {
                details = fmt::format(" ({})", event.details());
            }
            fmt::format_to(
                out,
                "  {:%T}.{:03d} {}{}\n",
                *tm,
                duration.count() % 1000,
                to_string(event.what()),
                details);
        }
    }
}

} // namespace cradle
