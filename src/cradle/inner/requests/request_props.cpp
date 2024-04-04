#include <spdlog/spdlog.h>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

default_retrier::default_retrier(int64_t base_millis, int max_attempts)
    : base_millis_{base_millis}, max_attempts_{max_attempts}
{
}

std::chrono::milliseconds
default_retrier::handle_exception(int attempt, std::exception const& exc) const
{
    auto logger{ensure_logger("retry")};

    // First decide whether this type of exception should lead to a retry at
    // all. For now, only http_request_failure may be retried.
    try
    {
        throw;
    }
    catch (http_request_failure const&)
    {
    }

    auto what{short_what(exc)};
    // Then decide whether the maximum number of attempts has been reached.
    if (attempt + 1 >= max_attempts_)
    {
        logger->error(
            "failed on attempt {}: {}; will not retry", attempt, what);
        throw;
    }

    // Finally, decide after how long the retry should happen;
    // exponential backoff.
    int64_t millis = base_millis_ * (int64_t{1} << (attempt * 2));
    logger->info(
        "failed on attempt {}: {}; will retry after {}ms",
        attempt,
        what,
        millis);
    return std::chrono::milliseconds{millis};
}

std::chrono::milliseconds
proxy_retrier::handle_exception(int attempt, std::exception const& exc) const
{
    int64_t const base_millis = 100;
    int const max_attempts = 9;
    auto logger{ensure_logger("retry")};

    // First decide whether this type of exception should lead to a retry at
    // all. Only retryable remote errors lead to a retry.
    try
    {
        throw;
    }
    catch (remote_error const& exc)
    {
        if (!exc.retryable())
        {
            throw;
        }
    }

    auto what{short_what(exc)};
    // Then decide whether the maximum number of attempts has been reached.
    if (attempt + 1 >= max_attempts)
    {
        logger->error(
            "failed on attempt {}: {}; will not retry", attempt, what);
        throw;
    }

    // Finally, decide after how long the retry should happen;
    // exponential backoff.
    int64_t millis = base_millis * (int64_t{1} << (attempt * 2));
    logger->info(
        "failed on attempt {}: {}; will retry after {}ms",
        attempt,
        what,
        millis);
    return std::chrono::milliseconds{millis};
}

} // namespace cradle
