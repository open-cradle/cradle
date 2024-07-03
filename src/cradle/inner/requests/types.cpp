#include <atomic>

#include <fmt/format.h>

#include <cradle/inner/requests/types.h>

namespace cradle {

bool
is_final(async_status s)
{
    return s == async_status::CANCELLED || s == async_status::FINISHED
           || s == async_status::ERROR;
}

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

catalog_id::catalog_id() noexcept
{
    static std::atomic<wrapped_t> next_dll_id{1};
    wrapped_ = next_dll_id.fetch_add(1, std::memory_order_relaxed);
}

} // namespace cradle
