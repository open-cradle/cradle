#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/resolve/seri_req.h>

#include "../../support/inner_service.h"

using namespace cradle;

namespace {

char const tag[] = "[inner][resolve][normalized_arg]";

request_uuid
make_test_uuid(std::string const& ext)
{
    return request_uuid{fmt::format("{}-{}", tag, ext)};
}

using func_props_t = request_props<caching_level_type::none, false, false>;
using coro_props_t = request_props<caching_level_type::none, true, false>;

int
plus_two_func(int x)
{
    return x + 2;
}

cppcoro::task<int>
plus_two_coro(context_intf& ctx, int x)
{
    co_return plus_two_func(x);
}

} // namespace

TEST_CASE("resolve serialized requests with normalized args", tag)
{
    non_caching_request_resolution_context ctx;
    seri_catalog cat{ctx.get_resources().get_seri_registry()};

    auto func_props{func_props_t{make_test_uuid("plus_two_func")}};
    auto coro_props{coro_props_t{make_test_uuid("plus_two_coro")}};
    // The framework should generate different uuid's for the requests created
    // by the two following normalize_arg calls, otherwise the second
    // register_resolver call will fail with a message like
    // "conflicting types for uuid normalization_uuid<int>".
    cat.register_resolver(rq_function_erased(
        func_props, plus_two_func, normalize_arg<int, func_props_t>(0)));
    cat.register_resolver(rq_function_erased(
        coro_props, plus_two_coro, normalize_arg<int, coro_props_t>(0)));

    // Function is "normal" (no coroutine); main request's arg is normalized
    auto req_a{rq_function_erased(
        func_props, plus_two_func, normalize_arg<int, func_props_t>(1))};
    std::string seri_req_a{serialize_request(req_a)};
    auto seri_resp_a{
        cppcoro::sync_wait(resolve_serialized_local(ctx, seri_req_a))};
    int resp_a{deserialize_response<int>(seri_resp_a.value())};
    seri_resp_a.on_deserialized();
    REQUIRE(resp_a == 3);

    // Function is "normal" (no coroutine); main request's arg is subrequest
    auto req_b{rq_function_erased(
        func_props,
        plus_two_func,
        rq_function_erased(
            func_props, plus_two_func, normalize_arg<int, func_props_t>(1)))};
    std::string seri_req_b{serialize_request(req_b)};
    auto seri_resp_b{
        cppcoro::sync_wait(resolve_serialized_local(ctx, seri_req_b))};
    int resp_b{deserialize_response<int>(seri_resp_b.value())};
    seri_resp_b.on_deserialized();
    REQUIRE(resp_b == 5);

    // Function is coroutine; main request's arg is normalized
    auto req_c{rq_function_erased(
        coro_props, plus_two_coro, normalize_arg<int, coro_props_t>(1))};
    std::string seri_req_c{serialize_request(req_c)};
    auto seri_resp_c{
        cppcoro::sync_wait(resolve_serialized_local(ctx, seri_req_c))};
    int resp_c{deserialize_response<int>(seri_resp_c.value())};
    seri_resp_c.on_deserialized();
    REQUIRE(resp_c == 3);

    // Function is coroutine; main request's arg is subrequest
    auto req_d{rq_function_erased(
        coro_props,
        plus_two_coro,
        rq_function_erased(
            coro_props, plus_two_coro, normalize_arg<int, coro_props_t>(1)))};
    std::string seri_req_d{serialize_request(req_d)};
    auto seri_resp_d{
        cppcoro::sync_wait(resolve_serialized_local(ctx, seri_req_d))};
    int resp_d{deserialize_response<int>(seri_resp_d.value())};
    seri_resp_d.on_deserialized();
    REQUIRE(resp_d == 5);
}
