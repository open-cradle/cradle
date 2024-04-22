#include <spdlog/spdlog.h>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

backoff_retrier_base::backoff_retrier_base(
    int64_t base_millis, int max_attempts)
    : base_millis_{base_millis}, max_attempts_{max_attempts}
{
}

void
backoff_retrier_base::save_retrier_state(
    JSONRequestOutputArchive& archive) const
{
    archive(cereal::make_nvp("base_millis", base_millis_));
    archive(cereal::make_nvp("max_attempts", max_attempts_));
}

void
backoff_retrier_base::load_retrier_state(JSONRequestInputArchive& archive)
{
    archive(cereal::make_nvp("base_millis", base_millis_));
    archive(cereal::make_nvp("max_attempts", max_attempts_));
}

std::chrono::milliseconds
backoff_retrier_base::attempt_retry(
    int attempt, std::exception const& exc) const
{
    auto logger{ensure_logger("retry")};

    auto what{short_what(exc)};
    // Decide whether the maximum number of attempts has been reached.
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

default_retrier::default_retrier(int64_t base_millis, int max_attempts)
    : backoff_retrier_base(base_millis, max_attempts)
{
}

std::chrono::milliseconds
default_retrier::handle_exception(int attempt, std::exception const& exc) const
{
    // First decide whether this type of exception should lead to a retry at
    // all. For now, only http_request_failure may be retried.
    try
    {
        throw;
    }
    catch (http_request_failure const&)
    {
    }

    // Try again if the maximum number of attempts has not been reached.
    return attempt_retry(attempt, exc);
}

proxy_retrier::proxy_retrier(int64_t base_millis, int max_attempts)
    : backoff_retrier_base(base_millis, max_attempts)
{
}

std::chrono::milliseconds
proxy_retrier::handle_exception(int attempt, std::exception const& exc) const
{
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

    // Try again if the maximum number of attempts has not been reached.
    return attempt_retry(attempt, exc);
}

} // namespace cradle
