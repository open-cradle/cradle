#include <regex>
#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_req.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>

#include "../../support/inner_service.h"

using namespace cradle;

namespace {

static char const tag[] = "[inner][resolve][seri_catalog]";

using required_ctx_types = ctx_type_list<testing_request_context>;
template<caching_level_type Level>
using local_props = request_props<Level, true, true, required_ctx_types>;

template<char const* arg>
class make_string
{
 public:
    cppcoro::task<std::string>
    operator()(context_intf& ctx) const
    {
        co_return arg;
    }
};

template<typename Function>
auto
rq_local(Function function, std::string const& title)
{
    constexpr auto level{caching_level_type::memory};
    return rq_function_erased(
        local_props<level>(request_uuid{title}, title), std::move(function));
}

} // namespace

TEST_CASE("register seri resolver and call it", tag)
{
    static char const arg[] = "a";
    auto req{rq_local(make_string<arg>{}, arg)};

    REQUIRE_NOTHROW(register_seri_resolver(req));

    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr, false, ""};
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
    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr, false, ""};
    std::string seri_req{serialize_request(req)};

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(seri_catalog::instance().resolve(ctx, seri_req)),
        Catch::StartsWith("no request registered with uuid"));
}

TEST_CASE("serialized request lacking uuid", tag)
{
    static char const arg[] = "c";
    auto req{rq_local(make_string<arg>{}, arg)};
    register_seri_resolver(req);
    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr, false, ""};
    std::string correct{serialize_request(req)};

    std::regex re("uuid");
    std::string wrong{std::regex_replace(correct, re, "wrong")};

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(seri_catalog::instance().resolve(ctx, wrong)),
        Catch::StartsWith("no uuid found in JSON"));
}

TEST_CASE("malformed serialized request", tag)
{
    static char const arg[] = "d";
    auto req{rq_local(make_string<arg>{}, arg)};
    register_seri_resolver(req);
    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr, false, ""};
    std::string seri_req{serialize_request(req)};

    seri_req.pop_back();

    REQUIRE_THROWS_WITH(
        cppcoro::sync_wait(seri_catalog::instance().resolve(ctx, seri_req)),
        Catch::StartsWith("rapidjson internal assertion failure"));
}

namespace {

cppcoro::task<std::string>
make_e_string(context_intf& ctx)
{
    co_return "e";
}

cppcoro::task<std::string>
make_f_string(context_intf& ctx)
{
    co_return "f";
}

} // namespace

TEST_CASE("resolve two C++ functions with the same signature", tag)
{
    auto req_e{rq_local(make_e_string, "test_seri_catalog_e")};
    auto req_f{rq_local(make_f_string, "test_seri_catalog_f")};

    REQUIRE_NOTHROW(register_seri_resolver(req_e));
    REQUIRE_NOTHROW(register_seri_resolver(req_f));

    std::string seri_req_e{serialize_request(req_e)};
    std::string seri_req_f{serialize_request(req_f)};

    REQUIRE(seri_req_e != seri_req_f);

    inner_resources resources;
    init_test_inner_service(resources);
    testing_request_context ctx{resources, nullptr, false, ""};

    auto seri_resp_e{
        cppcoro::sync_wait(seri_catalog::instance().resolve(ctx, seri_req_e))};
    auto seri_resp_f{
        cppcoro::sync_wait(seri_catalog::instance().resolve(ctx, seri_req_f))};
    REQUIRE(seri_resp_e.value() != seri_resp_f.value());

    std::string resp_e{deserialize_response<std::string>(seri_resp_e.value())};
    seri_resp_e.on_deserialized();
    std::string resp_f{deserialize_response<std::string>(seri_resp_f.value())};
    seri_resp_f.on_deserialized();

    REQUIRE(resp_e == "e");
    REQUIRE(resp_f == "f");
}
