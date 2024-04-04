#include <stdexcept>
#include <string>
#include <thread>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>

#include "../../support/inner_service.h"
#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/io/mock_http.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/resolve/resolve_request.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/utilities/errors.h>
#include <cradle/plugins/domain/testing/context.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][resolve][retry]";

request_uuid
make_test_uuid(int ext)
{
    return request_uuid{fmt::format("{}-{:04d}", tag, ext)};
}

auto
make_the_request()
{
    return make_get_request(
        "https://cradle.xyz/api/ask", {{"Accept", "text/plain"}});
}

auto
make_good_response()
{
    return make_http_200_response("42");
}

auto
make_bad_response()
{
    http_response response;
    response.status_code = 500;
    return response;
}

cppcoro::task<std::string>
ask_question(context_intf& ctx)
{
    auto& resources{ctx.get_resources()};
    auto response = co_await resources.async_http_request(make_the_request());
    if (response.status_code != 200)
    {
        CRADLE_THROW(
            http_request_failure() << internal_error_message_info(
                fmt::format("status_code {}", response.status_code)));
    }
    co_return to_string(response.body);
}

cppcoro::task<std::string>
concat_one_two(context_intf& ctx, std::string one, std::string two)
{
    co_return one + two;
}

// For successful retry:
// - num_bad == 1: one sub fails, one succeeds
// - num_good == 2: need one good response for each sub
void
setup_mock_http(inner_resources& resources, int num_bad = 1, int num_good = 2)
{
    auto& mock_http = resources.enable_http_mocking();
    mock_http_script script;
    for (int i = 0; i < num_bad; ++i)
    {
        script.push_back(
            mock_http_exchange{make_the_request(), make_bad_response()});
    }
    for (int i = 0; i < num_good; ++i)
    {
        script.push_back(
            mock_http_exchange{make_the_request(), make_good_response()});
    }
    mock_http.set_script(script);
}

template<typename Ctx>
void
test_retry(Ctx& ctx)
{
    setup_mock_http(ctx.get_resources());
    request_props<
        caching_level_type::none,
        request_function_t::coro,
        false,
        default_retrier>
        props{make_test_uuid(0)};
    auto req{rq_function(
        props,
        concat_one_two,
        rq_function(props, ask_question),
        rq_function(props, ask_question))};

    auto res = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(res == "4242");
}

} // namespace

TEST_CASE("resolve with retry - sync", tag)
{
    auto resources{make_inner_test_resources()};
    testing_request_context ctx{*resources, ""};
    test_retry(ctx);
}

TEST_CASE("resolve with retry - atst_context", tag)
{
    auto resources{make_inner_test_resources()};
    atst_context ctx{*resources};
    test_retry(ctx);
}

TEST_CASE("resolve with retry - root_local_atst_context", tag)
{
    auto resources{make_inner_test_resources()};
    auto tree_ctx{std::make_unique<local_tree_context_base>(*resources)};
    root_local_atst_context root_ctx{std::move(tree_ctx), nullptr};
    test_retry(root_ctx);
}

TEST_CASE("resolve with retry - too many failures", tag)
{
    int max_attempts = 2;
    // Both subs fail on the first attempt.
    // One sub also fails on the second attempt, exceeding max_attempts.
    int num_bad = 3;
    int num_good = 2;

    auto resources{make_inner_test_resources()};
    atst_context ctx{*resources};
    setup_mock_http(ctx.get_resources(), num_bad, num_good);
    // Not retrying a failed root request
    request_props<
        caching_level_type::none,
        request_function_t::coro,
        false,
        no_retrier>
        root_props{make_test_uuid(0)};
    request_props<
        caching_level_type::none,
        request_function_t::coro,
        false,
        default_retrier>
        sub_props{make_test_uuid(0), default_retrier(10, max_attempts)};
    auto req{rq_function(
        root_props,
        concat_one_two,
        rq_function(sub_props, ask_question),
        rq_function(sub_props, ask_question))};

    REQUIRE_THROWS_MATCHES(
        cppcoro::sync_wait(resolve_request(ctx, req)),
        http_request_failure,
        Catch::Predicate<std::exception>(
            [](std::exception const& exc) -> bool {
                return short_what(exc) == "status_code 500";
            }));
}

void
cancelling_func(async_context_intf& ctx)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    cppcoro::sync_wait(ctx.request_cancellation_coro());
}

TEST_CASE("resolve with retry - cancel retry", tag)
{
    // First attempt fails. Retry after 100ms would succeed, but the operation
    // is cancelled before it gets there.
    auto resources{make_inner_test_resources()};
    atst_context ctx{*resources};
    setup_mock_http(ctx.get_resources());
    request_props<
        caching_level_type::none,
        request_function_t::coro,
        false,
        default_retrier>
        props{make_test_uuid(0)};
    auto req{rq_function(
        props,
        concat_one_two,
        rq_function(props, ask_question),
        rq_function(props, ask_question))};
    std::jthread cancelling_thread(cancelling_func, std::ref(ctx));

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(resolve_request(ctx, req)), "operation cancelled");
    cancelling_thread.join();
}
