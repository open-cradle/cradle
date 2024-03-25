#include <stdexcept>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

std::chrono::milliseconds
default_retrier::prepare_retry(int attempt, std::string const& reason)
{
    int64_t base_millis = 100; // ms
    int64_t millis = base_millis * (4L << attempt);
    int max_attempts = 9;
    auto logger{ensure_logger("retry")};
    logger->info("resolve #{} failed: {}", attempt, reason);
    if (attempt + 1 >= max_attempts)
    {
        logger->warn("will not retry");
        throw std::runtime_error{
            fmt::format("giving up after {} attempts", attempt + 1)};
    }
    logger->info("will retry after {}ms", millis);
    return std::chrono::milliseconds{millis};
}

void
retrier_mixin<true>::set_retrier(std::shared_ptr<retrier_intf> retrier)
{
    retrier_ = std::move(retrier);
}

std::shared_ptr<retrier_intf>
retrier_mixin<true>::get_retrier() const
{
    if (!retrier_)
    {
        throw std::logic_error{"no retrier defined"};
    }
    return retrier_;
}

} // namespace cradle
