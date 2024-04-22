#include <chrono>
#include <exception>

#include <catch2/catch.hpp>

#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/utilities/errors.h>

using namespace cradle;

static char const tag[] = "[inner][requests][props]";

TEST_CASE("default_retrier rethrows unrecognized exception", tag)
{
    default_retrier retrier;
    try
    {
        throw std::logic_error{"test"};
    }
    catch (std::exception const& exc)
    {
        REQUIRE_THROWS_AS(retrier.handle_exception(0, exc), std::logic_error);
    }
}

TEST_CASE("default_retrier retries until max attempts", tag)
{
    constexpr int num_attempts{4};
    default_retrier retrier{1, num_attempts};
    constexpr int num_ok_attempts{num_attempts - 1};
    int64_t const expected_delays[num_ok_attempts] = {1, 4, 16};
    for (int attempt = 0; attempt < num_ok_attempts; ++attempt)
    {
        try
        {
            CRADLE_THROW(
                http_request_failure()
                << internal_error_message_info("the why"));
        }
        catch (std::exception const& exc)
        {
            REQUIRE(
                retrier.handle_exception(attempt, exc)
                == std::chrono::milliseconds{expected_delays[attempt]});
        }
    }
    try
    {
        CRADLE_THROW(
            http_request_failure() << internal_error_message_info("the why"));
    }
    catch (std::exception const& exc)
    {
        REQUIRE_THROWS_AS(
            retrier.handle_exception(num_ok_attempts, exc),
            http_request_failure);
    }
}

TEST_CASE("proxy_retrier rethrows unrecognized exception", tag)
{
    proxy_retrier retrier;
    try
    {
        CRADLE_THROW(
            http_request_failure() << internal_error_message_info("the why"));
    }
    catch (std::exception const& exc)
    {
        REQUIRE_THROWS_AS(
            retrier.handle_exception(0, exc), http_request_failure);
    }
}

TEST_CASE("proxy_retrier rethrows non-retryable remote_error", tag)
{
    proxy_retrier retrier;
    try
    {
        throw remote_error{"the what"};
    }
    catch (std::exception const& exc)
    {
        REQUIRE_THROWS_AS(retrier.handle_exception(0, exc), remote_error);
    }
}

TEST_CASE("proxy_retrier retries retryable remote_error", tag)
{
    proxy_retrier retrier;
    try
    {
        throw remote_error{"the what", "the msg", true};
    }
    catch (std::exception const& exc)
    {
        auto delay = retrier.handle_exception(0, exc);
        REQUIRE(delay > std::chrono::milliseconds(0));
    }
}

TEST_CASE("proxy_retrier gives up after too many attempts", tag)
{
    proxy_retrier retrier;
    try
    {
        throw remote_error{"the what", "the msg", true};
    }
    catch (std::exception const& exc)
    {
        REQUIRE_THROWS_AS(retrier.handle_exception(100, exc), remote_error);
    }
}
