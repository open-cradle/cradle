#include <regex>
#include <sstream>
#include <string>

#include <catch2/catch.hpp>
#include <cereal/archives/json.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>

#include "../../support/inner_service.h"
#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/resolve/seri_catalog.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][requests][function]";

static auto add2 = [](int a, int b) { return a + b; };
static auto mul2 = [](int a, int b) { return a * b; };

std::string
func_a()
{
    return "a";
}

std::string
func_b()
{
    return "b";
}

int
func_x(int x)
{
    return x;
}

static auto functor_a = []() { return func_a(); };
static auto functor_b = []() { return func_b(); };

cppcoro::task<std::string>
coro_a(context_intf& ctx)
{
    co_return "a";
}

cppcoro::task<std::string>
coro_b(context_intf& ctx)
{
    co_return "b";
}

// The type of this function's result depends on Function and Args types only,
// not on their values.
// It is not possible to distuinguish two result values created from different
// function and/or args values.
template<typename Function, typename... Args>
auto
make_lambda(Function function, Args... args)
{
    return [=]() { return function(args...); };
}

cppcoro::task<std::string>
make_string(context_intf& ctx, std::string val)
{
    co_return val;
}

request_uuid
make_test_uuid(std::string const& ext)
{
    return request_uuid{fmt::format("{}-{}", tag, ext)};
}

template<typename Value, typename Props>
std::string
get_unique_string(function_request_erased<Value, Props> const& req)
{
    unique_hasher hasher;
    req.update_hash(hasher);
    return hasher.get_string();
}

template<typename Value, typename Props>
std::string
to_json(function_request_erased<Value, Props> const& req)
{
    std::stringstream os;
    {
        cereal::JSONOutputArchive oarchive(os);
        req.save(oarchive);
    }
    return os.str();
}

} // namespace

TEST_CASE(
    "create function_request_erased: identical C++ functions, one uuid", tag)
{
    request_props<caching_level_type::memory> props{make_test_uuid("0000")};
    REQUIRE_NOTHROW(rq_function_erased(props, func_a));
    REQUIRE_NOTHROW(rq_function_erased(props, func_a));
}

#if 0
// TODO remove this test case and conflicting_functions_uuid_error if the check won't come back.
TEST_CASE(
    "create function_request_erased: different C++ functions, one uuid", tag)
{
    request_props<caching_level_type::memory> props{make_test_uuid("0001")};
    REQUIRE_NOTHROW(rq_function_erased(props, func_a));
    REQUIRE_THROWS_AS(
        rq_function_erased(props, func_b), conflicting_functions_uuid_error);
}
#endif

TEST_CASE("create function_request_erased: identical functors, one uuid", tag)
{
    request_props<caching_level_type::memory> props{make_test_uuid("0002")};
    REQUIRE_NOTHROW(rq_function_erased(props, functor_a));
    REQUIRE_NOTHROW(rq_function_erased(props, functor_a));
}

TEST_CASE("create function_request_erased: different functors, one uuid", tag)
{
    // This is a valid use case when dynamically loading shared libraries.
    request_props<caching_level_type::memory> props{make_test_uuid("0003")};
    REQUIRE_NOTHROW(rq_function_erased(props, functor_a));
    REQUIRE_NOTHROW(rq_function_erased(props, functor_b));
}

TEST_CASE(
    "function_request_erased: identical capturing lambdas, one uuid", tag)
{
    request_props<caching_level_type::memory> props{make_test_uuid("0004")};
    auto lambda_a0{make_lambda(func_a)};
    auto lambda_a1{make_lambda(func_a)};
    auto req_a0{rq_function_erased(props, lambda_a0)};
    auto req_a1{rq_function_erased(props, lambda_a1)};

    REQUIRE(req_a0 == req_a1);
    REQUIRE(!(req_a0 < req_a1));
    REQUIRE(!(req_a1 < req_a0));
    REQUIRE(req_a0.hash() == req_a1.hash());

    non_caching_request_resolution_context ctx;
    auto result_a0{cppcoro::sync_wait(req_a0.resolve_sync(ctx))};
    auto result_a1{cppcoro::sync_wait(req_a1.resolve_sync(ctx))};

    REQUIRE(result_a0 == "a");
    REQUIRE(result_a1 == "a");
}

TEST_CASE(
    "function_request_erased: lambdas capturing different functions, "
    "one uuid",
    tag)
{
    // This is legal if the two lambdas come from different DLLs (and their
    // implementations are identical). The two requests should resolve to the
    // specified values.
    request_props<caching_level_type::memory> props{make_test_uuid("0005")};
    auto lambda_a{make_lambda(func_a)};
    auto lambda_b{make_lambda(func_b)};
    auto req_a{rq_function_erased(props, lambda_a)};
    auto req_b{rq_function_erased(props, lambda_b)};

    REQUIRE(req_a == req_b);
    REQUIRE(!(req_a < req_b));
    REQUIRE(!(req_b < req_a));
    REQUIRE(req_a.hash() == req_b.hash());

    non_caching_request_resolution_context ctx;
    auto result_a{cppcoro::sync_wait(req_a.resolve_sync(ctx))};
    auto result_b{cppcoro::sync_wait(req_b.resolve_sync(ctx))};

    REQUIRE(result_a == "a");
    REQUIRE(result_b == "b");
}

TEST_CASE(
    "function_request_erased: lambdas capturing different args, one "
    "uuid",
    tag)
{
    // A variant on the previous test case.
    request_props<caching_level_type::memory> props{make_test_uuid("0006")};
    auto lambda_a{make_lambda(func_x, 2)};
    auto lambda_b{make_lambda(func_x, 3)};
    auto req_a{rq_function_erased(props, lambda_a)};
    auto req_b{rq_function_erased(props, lambda_b)};

    REQUIRE(req_a == req_b);
    REQUIRE(!(req_a < req_b));
    REQUIRE(!(req_b < req_a));
    REQUIRE(req_a.hash() == req_b.hash());

    non_caching_request_resolution_context ctx;
    auto result_a{cppcoro::sync_wait(req_a.resolve_sync(ctx))};
    auto result_b{cppcoro::sync_wait(req_b.resolve_sync(ctx))};

    REQUIRE(result_a == 2);
    REQUIRE(result_b == 3);
}

TEST_CASE(
    "compare function_request_erased: indistinguishable lambdas, different "
    "uuids",
    tag)
{
    request_props<caching_level_type::memory> props_a{make_test_uuid("0010")};
    request_props<caching_level_type::memory> props_b{make_test_uuid("0011")};
    auto lambda_a{make_lambda(func_a)};
    auto lambda_b{make_lambda(func_b)};
    auto req_a{rq_function_erased(props_a, lambda_a)};
    auto req_b{rq_function_erased(props_b, lambda_b)};

    // The two requests are based on different uuids so differ.
    REQUIRE(req_a != req_b);
    REQUIRE((req_a < req_b || req_b < req_a));
    // A hash collision is possible but very unlikely.
    REQUIRE(req_a.hash() != req_b.hash());

    non_caching_request_resolution_context ctx;
    auto result_a{cppcoro::sync_wait(req_a.resolve_sync(ctx))};
    auto result_b{cppcoro::sync_wait(req_b.resolve_sync(ctx))};

    REQUIRE(result_a == "a");
    REQUIRE(result_b == "b");
}

TEST_CASE("compare function_request_erased with subrequest", tag)
{
    request_props<caching_level_type::memory> props0{make_test_uuid("0030")};
    auto req0a{rq_function_erased(props0, add2, 1, 2)};
    auto req0b{rq_function_erased(props0, add2, 1, 2)};

    REQUIRE(req0a == req0b);
    REQUIRE(!(req0a < req0b));
    REQUIRE(!(req0b < req0a));

    request_props<caching_level_type::memory> props1{make_test_uuid("0031")};
    auto req1a{rq_function_erased(props1, add2, req0a, 3)};
    auto req1b{rq_function_erased(props1, add2, req0b, 3)};
    REQUIRE(req1a == req1b);
    REQUIRE(!(req1a < req1b));
    REQUIRE(!(req1b < req1a));

    // Shouldn't assert in function_request_impl::equals()
    REQUIRE(req0a != req1a);
    REQUIRE((req0a < req1a || req1a < req0a));
}

template<typename A, typename B>
    requires TypedArg<A, int> && TypedArg<B, int>
static auto
rq_0022(A a, B b)
{
    using props_type = request_props<caching_level_type::full>;
    using arg_props_type = request_props<caching_level_type::memory>;
    return rq_function_erased(
        props_type{make_test_uuid("0022")},
        add2,
        normalize_arg<int, arg_props_type>(std::move(a)),
        normalize_arg<int, arg_props_type>(std::move(b)));
}

TEST_CASE(
    "function_request_erased identity: subrequests with different functors",
    tag)
{
    seri_registry registry;
    seri_catalog cat{registry};
    using props0_type = request_props<caching_level_type::memory>;
    props0_type props0a{make_test_uuid("0020")};
    props0_type props0b{make_test_uuid("0021")};
    auto req0a{rq_function_erased(props0a, add2, 1, 2)};
    auto req0b{rq_function_erased(props0b, mul2, 1, 2)};
    cat.register_resolver(req0a);
    cat.register_resolver(req0b);

    REQUIRE(!req0a.equals(req0b));
    REQUIRE((req0a.less_than(req0b) || req0b.less_than(req0a)));

    cat.register_resolver(rq_0022(0, 1));
    auto req1a{rq_0022(req0a, 3)};
    auto req1b{rq_0022(req0b, 3)};

    REQUIRE(!req1a.equals(req1b));
    REQUIRE((req1a.less_than(req1b) || req1b.less_than(req1a)));
    REQUIRE(get_unique_string(req1a) != get_unique_string(req1b));
    REQUIRE(to_json(req1a) != to_json(req1b));
}

TEST_CASE("compare function_request_erased: one C++ function", tag)
{
    request_props<caching_level_type::memory> props{make_test_uuid("0040")};
    auto req_a{rq_function_erased(props, func_a)};

    REQUIRE(req_a.equals(req_a));

    REQUIRE(!req_a.less_than(req_a));
}

TEST_CASE("compare function_request_erased: identical C++ functions", tag)
{
    constexpr bool is_coro{true};
    request_props<caching_level_type::memory, is_coro> props{
        make_test_uuid("0050")};
    auto req_a0{rq_function_erased(props, coro_a)};
    auto req_a1{rq_function_erased(props, coro_a)};

    REQUIRE(req_a0.equals(req_a1));

    REQUIRE(!req_a0.less_than(req_a1));

    REQUIRE(req_a0.hash() == req_a1.hash());

    REQUIRE(get_unique_string(req_a0) == get_unique_string(req_a1));
}

TEST_CASE("compare function_request_erased: different C++ functions", tag)
{
    constexpr bool is_coro{true};
    request_props<caching_level_type::memory, is_coro> props_a{
        make_test_uuid("0060")};
    request_props<caching_level_type::memory, is_coro> props_b{
        make_test_uuid("0061")};
    // req_a and req_b have the same signature (type), but refer to different
    // C++ functions.
    auto req_a{rq_function_erased(props_a, coro_a)};
    auto req_b{rq_function_erased(props_b, coro_b)};

    REQUIRE(!req_a.equals(req_b));

    REQUIRE((req_a.less_than(req_b) || req_b.less_than(req_a)));

    // The hashes could theoretically be equal but that's highly unlikely.
    REQUIRE(req_a.hash() != req_b.hash());

    REQUIRE(get_unique_string(req_a) != get_unique_string(req_b));
}

TEST_CASE(
    "compare function_request_erased: C++ functions with different args", tag)
{
    constexpr bool is_coro{true};
    request_props<caching_level_type::memory, is_coro> props{
        make_test_uuid("0070")};
    // req_a and req_b have the same signature (type), refer to different
    // C++ functions, but take different args.
    auto req_a{rq_function_erased(props, make_string, std::string{"a"})};
    auto req_b{rq_function_erased(props, make_string, std::string{"b"})};

    REQUIRE(!req_a.equals(req_b));

    REQUIRE((req_a.less_than(req_b) || req_b.less_than(req_a)));

    // The hashes could still be equal but that's highly unlikely.
    REQUIRE(req_a.hash() != req_b.hash());

    REQUIRE(get_unique_string(req_a) != get_unique_string(req_b));
}

TEST_CASE("function_request_impl: load unregistered function", tag)
{
    std::string good_uuid_str{"before_0100_after"};
    std::string bad_uuid_str{"before_0101_after"};
    auto good_uuid{make_test_uuid(good_uuid_str)};
    auto bad_uuid{make_test_uuid(bad_uuid_str)};
    request_props<caching_level_type::memory> props{good_uuid};
    using value_type = std::string;
    using props_type = decltype(props);
    using impl_type = function_request_impl<
        value_type,
        props_type::func_is_coro,
        decltype(&func_a)>;

    auto good_impl{std::make_shared<impl_type>(good_uuid, func_a)};
    std::stringstream os;
    {
        cereal::JSONOutputArchive oarchive(os);
        // Cannot do oarchive(good_impl) due to ambiguity between
        //   function_request_impl::save()
        // and the global
        //   save(Archive& archive, function_request_intf<Value> const& intf)
        good_impl->save(oarchive);
    }
    std::string good_seri = os.str();

    std::shared_ptr<impl_type> bad_impl{std::make_shared<impl_type>(bad_uuid)};
    auto bad_seri{std::regex_replace(
        good_seri, std::regex{good_uuid_str}, bad_uuid_str)};
    std::istringstream is(bad_seri);
    cereal::JSONInputArchive iarchive(is);
    REQUIRE_THROWS_AS(bad_impl->load(iarchive), unregistered_uuid_error);
}
