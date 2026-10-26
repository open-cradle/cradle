#include <cradle/thinknode/service/core.h>

#include <chrono>
#include <thread>

#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/operation_cancelled.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

#include "../../support/thinknode.h"
#include <cradle/typing/io/http_requests.hpp>
#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("I/O timer scheduling", "[service][core][timer]")
{
    using namespace std::chrono_literals;

    io_service service;
    std::jthread worker([&] { service.process_events(); });
    auto stop_service = cppcoro::on_scope_exit([&] { service.stop(); });
    auto wait = [&](std::chrono::milliseconds delay,
                    cppcoro::cancellation_token token = {})
        -> cppcoro::task<void> {
        co_await service.schedule_after(delay, token);
    };

    SECTION("timer expires")
    {
        auto start = std::chrono::steady_clock::now();
        cppcoro::sync_wait(wait(5ms));
        REQUIRE(std::chrono::steady_clock::now() - start >= 5ms);
    }

    SECTION("zero delay")
    {
        REQUIRE_NOTHROW(cppcoro::sync_wait(wait(0ms)));
    }

    SECTION("cancellation before scheduling")
    {
        cppcoro::cancellation_source source;
        source.request_cancellation();
        REQUIRE_THROWS_AS(
            cppcoro::sync_wait(wait(200ms, source.token())),
            cppcoro::operation_cancelled);
    }

    SECTION("cancellation while suspended")
    {
        cppcoro::cancellation_source source;
        auto cancel = [&]() -> cppcoro::task<void> {
            co_await service.schedule_after(5ms);
            source.request_cancellation();
        };
        REQUIRE_THROWS_AS(
            cppcoro::sync_wait(cppcoro::when_all(
                wait(200ms, source.token()), cancel())),
            cppcoro::operation_cancelled);
    }

    SECTION("cancellation from another thread")
    {
        cppcoro::cancellation_source source;
        std::jthread canceller([&] {
            std::this_thread::sleep_for(5ms);
            source.request_cancellation();
        });
        auto start = std::chrono::steady_clock::now();
        REQUIRE_THROWS_AS(
            cppcoro::sync_wait(wait(5000ms, source.token())),
            cppcoro::operation_cancelled);
        REQUIRE(std::chrono::steady_clock::now() - start < 1s);
    }
}

TEST_CASE("HTTP requests", "[service][core]")
{
    auto resources{make_thinknode_test_resources()};

    auto async_response = resources->async_http_request(make_get_request(
        "https://postman-echo.com/get?color=navy", http_header_list()));

    auto response = cppcoro::sync_wait(async_response);
    REQUIRE(response.status_code == 200);
    auto body = parse_json_response(response);
    REQUIRE(
        get_field(cast<dynamic_map>(body), "args")
        == dynamic({{dynamic("color"), dynamic("navy")}}));
}
