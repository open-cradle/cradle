#include <cstring>
#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/encodings/msgpack_value.h>
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

using func_props_t = request_props<
    caching_level_type::none,
    request_function_t::plain,
    false>;
using coro_props_t
    = request_props<caching_level_type::none, request_function_t::coro, false>;

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
    auto resources{make_inner_test_resources()};
    non_caching_request_resolution_context ctx{*resources};
    seri_catalog cat{resources->get_seri_registry()};

    auto func_props{func_props_t{make_test_uuid("plus_two_func")}};
    auto coro_props{coro_props_t{make_test_uuid("plus_two_coro")}};
    // The framework should generate different uuid's for the requests created
    // by the two following normalize_arg calls, otherwise the second
    // register_resolver call will fail with a message like
    // "conflicting types for uuid normalization_uuid<int>".
    cat.register_resolver(rq_function(
        func_props, plus_two_func, normalize_arg<int, func_props_t>(0)));
    cat.register_resolver(rq_function(
        coro_props, plus_two_coro, normalize_arg<int, coro_props_t>(0)));

    // Function is "normal" (no coroutine); main request's arg is normalized
    auto req_a{rq_function(
        func_props, plus_two_func, normalize_arg<int, func_props_t>(1))};
    std::string seri_req_a{serialize_request(req_a)};
    auto seri_resp_a{
        cppcoro::sync_wait(resolve_serialized_local(ctx, seri_req_a))};
    int resp_a{deserialize_value<int>(seri_resp_a.value())};
    seri_resp_a.on_deserialized();
    REQUIRE(resp_a == 3);

    // Function is "normal" (no coroutine); main request's arg is subrequest
    auto req_b{rq_function(
        func_props,
        plus_two_func,
        rq_function(
            func_props, plus_two_func, normalize_arg<int, func_props_t>(1)))};
    std::string seri_req_b{serialize_request(req_b)};
    auto seri_resp_b{
        cppcoro::sync_wait(resolve_serialized_local(ctx, seri_req_b))};
    int resp_b{deserialize_value<int>(seri_resp_b.value())};
    seri_resp_b.on_deserialized();
    REQUIRE(resp_b == 5);

    // Function is coroutine; main request's arg is normalized
    auto req_c{rq_function(
        coro_props, plus_two_coro, normalize_arg<int, coro_props_t>(1))};
    std::string seri_req_c{serialize_request(req_c)};
    auto seri_resp_c{
        cppcoro::sync_wait(resolve_serialized_local(ctx, seri_req_c))};
    int resp_c{deserialize_value<int>(seri_resp_c.value())};
    seri_resp_c.on_deserialized();
    REQUIRE(resp_c == 3);

    // Function is coroutine; main request's arg is subrequest
    auto req_d{rq_function(
        coro_props,
        plus_two_coro,
        rq_function(
            coro_props, plus_two_coro, normalize_arg<int, coro_props_t>(1)))};
    std::string seri_req_d{serialize_request(req_d)};
    auto seri_resp_d{
        cppcoro::sync_wait(resolve_serialized_local(ctx, seri_req_d))};
    int resp_d{deserialize_value<int>(seri_resp_d.value())};
    seri_resp_d.on_deserialized();
    REQUIRE(resp_d == 5);
}

TEST_CASE("normalized C-string arg stored as std::string", tag)
{
    auto resources{make_inner_test_resources()};
    non_caching_request_resolution_context ctx{*resources};

    auto function = [](std::string const& arg) { return arg; };
    auto func_props{func_props_t{make_test_uuid("identity")}};
    char arg_string[] = "original";
    auto req{rq_function(
        func_props,
        function,
        normalize_arg<std::string, func_props_t>(arg_string))};

    auto res0 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(res0 == "original");

    std::strcpy(arg_string, "changed");
    auto res1 = cppcoro::sync_wait(resolve_request(ctx, req));
    REQUIRE(res1 == "original");
}

// A proxy subrequest should serialize to the same value as a corresponding
// function subrequest.
// A proxy subrequest is possible for a proxy main request, but not for a
// function main request (e.g. because it cannot be hashed).
// A function subrequest is possible for either type of main request.
// The two normalize_arg() calls pass their arguments unchanged.
TEST_CASE("compare normalized proxy/function requests", tag)
{
    using proxy_props_t = request_props<
        caching_level_type::none,
        request_function_t::proxy_plain,
        false>;
    proxy_props_t proxy_main_props{request_uuid{"main"}};
    func_props_t func_subreq_props{request_uuid{"sub"}};
    proxy_props_t proxy_subreq_props{request_uuid{"sub"}};

    auto func_subreq{rq_function(func_subreq_props, plus_two_func, 17)};
    auto proxy_subreq{rq_proxy<int>(proxy_subreq_props, 17)};

    auto req_a{rq_proxy<int>(
        proxy_main_props, normalize_arg<int, proxy_props_t>(func_subreq))};
    auto req_b{rq_proxy<int>(
        proxy_main_props, normalize_arg<int, proxy_props_t>(proxy_subreq))};

    std::string seri_req_a{serialize_request(req_a)};
    std::string seri_req_b{serialize_request(req_b)};

    REQUIRE(seri_req_a == seri_req_b);
}

TEST_CASE("compare normalized proxy/coroutine requests", tag)
{
    using proxy_props_t = request_props<
        caching_level_type::none,
        request_function_t::proxy_coro,
        false>;
    proxy_props_t proxy_main_props{request_uuid{"main"}};
    coro_props_t coro_subreq_props{request_uuid{"sub"}};
    proxy_props_t proxy_subreq_props{request_uuid{"sub"}};

    auto coro_subreq{rq_function(coro_subreq_props, plus_two_coro, 19)};
    auto proxy_subreq{rq_proxy<int>(proxy_subreq_props, 19)};

    auto req_a{rq_proxy<int>(
        proxy_main_props, normalize_arg<int, proxy_props_t>(coro_subreq))};
    auto req_b{rq_proxy<int>(
        proxy_main_props, normalize_arg<int, proxy_props_t>(proxy_subreq))};

    std::string seri_req_a{serialize_request(req_a)};
    std::string seri_req_b{serialize_request(req_b)};

    REQUIRE(seri_req_a == seri_req_b);
}
