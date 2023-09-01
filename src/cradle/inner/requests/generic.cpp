#include <fmt/format.h>

#include <cradle/inner/requests/generic.h>

namespace cradle {

std::string
to_string(async_status s)
{
    char const* res = nullptr;
    switch (s)
    {
        case async_status::CREATED:
            res = "CREATED";
            break;
        case async_status::SUBS_RUNNING:
            res = "SUBS_RUNNING";
            break;
        case async_status::SELF_RUNNING:
            res = "SELF_RUNNING";
            break;
        case async_status::CANCELLED:
            res = "CANCELLED";
            break;
        case async_status::AWAITING_RESULT:
            res = "AWAITING_RESULT";
            break;
        case async_status::FINISHED:
            res = "FINISHED";
            break;
        case async_status::ERROR:
            res = "ERROR";
            break;
        default:
            return fmt::format("bad async_status {}", static_cast<int>(s));
    }
    return std::string{res};
}

} // namespace cradle
