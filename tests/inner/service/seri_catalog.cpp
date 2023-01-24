#include <regex>
#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/seri_catalog.h>
#include <cradle/inner/service/seri_req.h>
#include <cradle/plugins/domain/testing/context.h>

#include "../../support/inner_service.h"

using namespace cradle;

namespace {

static char const tag[] = "[inner][service][seri_catalog]";

template<caching_level_type Level>
using local_props = request_props<Level, true, true, testing_request_context>;

// TODO same-typed raw functions would be treated identically in the cereal
// part of register_polymorphic_type()
template<char const* arg>
class make_string
{
 public:
    cppcoro::task<std::string>
    operator()(testing_request_context& ctx) const
    {
        co_return arg;
    }
};

template<typename Function>
auto
rq_local(Function function, std::string const& title)
{
    constexpr auto level{caching_level_type::memory};
    return rq_function_erased_coro<std::string>(
        local_props<level>(request_uuid{title}, title), std::move(function));
}

} // namespace

TEST_CASE("register seri resolver and call it", tag)
{
    static char const arg[] = "a";
    auto req{rq_local(make_string<arg>{}, arg)};

    REQUIRE_NOTHROW(register_seri_resolver<testing_request_context>(req));

    inner_resources service;
    init_test_inner_service(service);
    testing_request_context ctx{service, nullptr};
    std::string seri_req{serialize_request(req)};
    auto seri_resp{
        cppcoro::sync_wait(seri_catalog::instance().resolve(ctx, seri_req))};
    std::string response{deserialize_response<std::string>(seri_resp.value())};
    seri_resp.on_deserialized();

    REQUIRE(response == "a");
}

TEST_CASE("call unregistered resolver", tag)
{
    static char const arg[] = "b";
    auto req{rq_local(make_string<arg>{}, arg)};
    inner_resources service;
    init_test_inner_service(service);
    testing_request_context ctx{service, nullptr};
    std::string seri_req{serialize_request(req)};

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(seri_catalog::instance().resolve(ctx, seri_req)),
        Catch::StartsWith("no request registered with uuid"));
}

TEST_CASE("serialized request lacking polymorphic_name", tag)
{
    static char const arg[] = "c";
    auto req{rq_local(make_string<arg>{}, arg)};
    register_seri_resolver<testing_request_context>(req);
    inner_resources service;
    init_test_inner_service(service);
    testing_request_context ctx{service, nullptr};
    std::string correct{serialize_request(req)};

    std::regex re("polymorphic_name");
    std::string wrong{std::regex_replace(correct, re, "wrong")};

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(seri_catalog::instance().resolve(ctx, wrong)),
        Catch::StartsWith("no polymorphic_name found in JSON"));
}

TEST_CASE("malformed serialized request", tag)
{
    static char const arg[] = "d";
    auto req{rq_local(make_string<arg>{}, arg)};
    register_seri_resolver<testing_request_context>(req);
    inner_resources service;
    init_test_inner_service(service);
    testing_request_context ctx{service, nullptr};
    std::string seri_req{serialize_request(req)};

    seri_req.pop_back();

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(seri_catalog::instance().resolve(ctx, seri_req)),
        Catch::StartsWith("rapidjson internal assertion failure"));
}
