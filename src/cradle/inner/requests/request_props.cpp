#include <stdexcept>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

// TODO parameterize base_millis / max_attempts, saving them in serialization
// Returns the time to wait before the next retry, or throws if the maximum
// number of retries has been exceeded.
std::chrono::milliseconds
default_retrier::prepare_retry(int attempt, std::string const& reason) const
{
    int64_t base_millis = 100; // ms
    // Exponential backoff
    int64_t millis = base_millis * (4L << attempt);
    int max_attempts = 9;
    auto logger{ensure_logger("retry")};
    logger->info("resolve #{} failed: {}", attempt, reason);
    if (attempt + 1 >= max_attempts)
    {
        logger->warn("will not retry");
        // TODO dedicated exception class
        throw std::runtime_error{
            fmt::format("giving up after {} attempts", attempt + 1)};
    }
    logger->info("will retry after {}ms", millis);
    return std::chrono::milliseconds{millis};
}

} // namespace cradle
