#include <stdexcept>
#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>

#include "../../support/inner_service.h"
#include <cradle/inner/io/mock_http.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/resolve/resolve_retry.h>
#include <cradle/inner/service/resources.h>
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
        "https://gradle.xyz/api/ask", {{"Accept", "text/plain"}});
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
        throw std::runtime_error(
            fmt::format("status_code {}", response.status_code));
    }
    co_return to_string(response.body);
}

cppcoro::task<std::string>
concat_one_two(context_intf& ctx, std::string one, std::string two)
{
    co_return one + two;
}

template<typename Ctx>
void
test_retry(Ctx& ctx)
{
    auto& resources{ctx.get_resources()};

    auto& mock_http = resources.enable_http_mocking();
    mock_http_script script;
    // One sub fails, one succeeds
    for (int i = 0; i < 1; ++i)
    {
        script.push_back(
            mock_http_exchange{make_the_request(), make_bad_response()});
    }
    // TODO one good response will be discarded and recalculated;
    // might be avoidable for async
    for (int i = 0; i < 3; ++i)
    {
        script.push_back(
            mock_http_exchange{make_the_request(), make_good_response()});
    }
    mock_http.set_script(script);

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

    auto res = cppcoro::sync_wait(resolve_request_with_retry(ctx, req));
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
