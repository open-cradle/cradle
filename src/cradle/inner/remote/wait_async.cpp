#include <algorithm>
#include <chrono>
#include <thread>

#include <fmt/format.h>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/remote/wait_async.h>
#include <cradle/inner/requests/generic.h>

namespace cradle {

async_status_matcher::async_status_matcher(
    std::string name, spdlog::logger& logger)
    : name_{std::move(name)}, logger_{logger}
{
}

void
async_status_matcher::report_status(async_status status, bool done) const
{
    if (!done)
    {
        logger_.debug("{}: status {}, NOT done", name_, status);
    }
    else
    {
        logger_.debug("{}: status {}, DONE", name_, status);
    }
}

void
wait_until_async_status_matches(
    remote_proxy& proxy,
    async_id remote_id,
    async_status_matcher const& matcher)
{
    int sleep_time{1};
    for (;;)
    {
        auto status = proxy.get_async_status(remote_id);
        if (matcher(status))
        {
            break;
        }
        else if (status == async_status::CANCELLED)
        {
            throw async_cancelled(
                fmt::format("remote async {} cancelled", remote_id));
        }
        else if (status == async_status::ERROR)
        {
            std::string errmsg = proxy.get_async_error_message(remote_id);
            throw async_error(errmsg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        sleep_time = std::min((sleep_time + 1) * 3 / 2, 100);
    }
}

} // namespace cradle
