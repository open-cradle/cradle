#include <regex>
#include <sstream>
#include <string>

#include <catch2/catch.hpp>
#include <cereal/archives/json.hpp>

#include <cradle/inner/requests/function.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][requests][function]";

static auto add2 = [](int a, int b) { return a + b; };

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

static auto functor_a = []() { return func_a(); };
static auto functor_b = []() { return func_b(); };

cppcoro::task<std::string>
coro_a(cached_context_intf& ctx)
{
    co_return "a";
}

cppcoro::task<std::string>
coro_b(cached_context_intf& ctx)
{
    co_return "b";
}

cppcoro::task<std::string>
make_string(cached_context_intf& ctx, std::string val)
{
    co_return val;
}

request_uuid
make_test_uuid(std::string const& ext)
{
    return request_uuid{fmt::format("{}-{}", tag, ext)};
}

} // namespace

TEST_CASE(
    "create function_request_erased: identical C++ functions, one uuid", tag)
{
    request_props<caching_level_type::memory> props{make_test_uuid("0000")};
    REQUIRE_NOTHROW(rq_function_erased(props, func_a));
    REQUIRE_NOTHROW(rq_function_erased(props, func_a));
}

TEST_CASE(
    "create function_request_erased: different C++ functions, one uuid", tag)
{
    request_props<caching_level_type::memory> props{make_test_uuid("0001")};
    REQUIRE_NOTHROW(rq_function_erased(props, func_a));
    REQUIRE_THROWS_AS(
        rq_function_erased(props, func_b), conflicting_functions_uuid_error);
}

TEST_CASE("create function_request_erased: identical functors, one uuid", tag)
{
    request_props<caching_level_type::memory> props{make_test_uuid("0002")};
    REQUIRE_NOTHROW(rq_function_erased(props, functor_a));
    REQUIRE_NOTHROW(rq_function_erased(props, functor_a));
}

TEST_CASE("create function_request_erased: different functors, one uuid", tag)
{
    request_props<caching_level_type::memory> props{make_test_uuid("0003")};
    REQUIRE_NOTHROW(rq_function_erased(props, functor_a));
    REQUIRE_THROWS_AS(
        rq_function_erased(props, functor_b), conflicting_types_uuid_error);
}

TEST_CASE("create function_request_erased: fully cached, no uuid", tag)
{
    request_props<caching_level_type::full> props;
    REQUIRE_THROWS_AS(rq_function_erased(props, func_a), missing_uuid_error);
}

TEST_CASE("compare function_request_erased with subrequest", tag)
{
    request_props<caching_level_type::memory> props;
    auto req0a{rq_function_erased(props, add2, 1, 2)};
    auto req0b{rq_function_erased(props, add2, 1, 2)};

    REQUIRE(req0a == req0b);
    REQUIRE(!(req0a < req0b));
    REQUIRE(!(req0b < req0a));

    auto req1a{rq_function_erased(props, add2, req0a, 3)};
    auto req1b{rq_function_erased(props, add2, req0b, 3)};
    REQUIRE(req1a == req1b);
    REQUIRE(!(req1a < req1b));
    REQUIRE(!(req1b < req1a));

    // Shouldn't assert in function_request_impl::equals()
    REQUIRE(req0a != req1a);
    REQUIRE((req0a < req1a || req1a < req0a));
}

TEST_CASE("compare function_request_erased: one C++ function", tag)
{
    request_props<caching_level_type::memory> props;
    auto req_a{rq_function_erased(props, func_a)};

    REQUIRE(req_a.equals(req_a));

    REQUIRE(!req_a.less_than(req_a));
}

TEST_CASE("compare function_request_erased: identical C++ functions", tag)
{
    constexpr bool is_coro{true};
    request_props<caching_level_type::memory, is_coro> props;
    auto req_a0{rq_function_erased_coro<std::string>(props, coro_a)};
    auto req_a1{rq_function_erased_coro<std::string>(props, coro_a)};

    REQUIRE(req_a0.equals(req_a1));

    REQUIRE(!req_a0.less_than(req_a1));

    REQUIRE(req_a0.hash() == req_a1.hash());

    unique_hasher hasher_a0;
    unique_hasher hasher_a1;
    req_a0.update_hash(hasher_a0);
    req_a1.update_hash(hasher_a1);
    REQUIRE(hasher_a0.get_string() == hasher_a1.get_string());
}

TEST_CASE("compare function_request_erased: different C++ functions", tag)
{
    constexpr bool is_coro{true};
    request_props<caching_level_type::memory, is_coro> props;
    // req_a and req_b have the same signature (type), but refer to different
    // C++ functions.
    auto req_a{rq_function_erased_coro<std::string>(props, coro_a)};
    auto req_b{rq_function_erased_coro<std::string>(props, coro_b)};

    REQUIRE(!req_a.equals(req_b));

    REQUIRE((req_a.less_than(req_b) || req_b.less_than(req_a)));

    // The hashes could still be equal but that's highly unlikely.
    REQUIRE(req_a.hash() != req_b.hash());

    // req_a and req_b differ in their C++ function only. In the context of a
    // unique hash, this difference would be reflected in the uuid of a root
    // request. req_a and req_b have no uuid, thus are no root requests, and
    // their unique hash values should be the same.
    unique_hasher hasher_a;
    unique_hasher hasher_b;
    req_a.update_hash(hasher_a);
    req_b.update_hash(hasher_b);
    REQUIRE(hasher_a.get_string() == hasher_b.get_string());
}

TEST_CASE(
    "compare function_request_erased: C++ functions with different args", tag)
{
    constexpr bool is_coro{true};
    request_props<caching_level_type::memory, is_coro> props;
    // req_a and req_b have the same signature (type), refer to different
    // C++ functions, but take different args.
    auto req_a{rq_function_erased_coro<std::string>(
        props, make_string, std::string{"a"})};
    auto req_b{rq_function_erased_coro<std::string>(
        props, make_string, std::string{"b"})};

    REQUIRE(!req_a.equals(req_b));

    REQUIRE((req_a.less_than(req_b) || req_b.less_than(req_a)));

    // The hashes could still be equal but that's highly unlikely.
    REQUIRE(req_a.hash() != req_b.hash());

    unique_hasher hasher_a;
    unique_hasher hasher_b;
    req_a.update_hash(hasher_a);
    req_b.update_hash(hasher_b);
    REQUIRE(hasher_a.get_string() != hasher_b.get_string());
}

TEST_CASE("function_request_impl: load unregistered function", tag)
{
    std::string good_uuid_str{"before_0100_after"};
    std::string bad_uuid_str{"before_0101_after"};
    auto good_uuid{make_test_uuid(good_uuid_str)};
    request_props<caching_level_type::memory> props{good_uuid};
    using value_type = std::string;
    using props_type = decltype(props);
    using ctx_type = typename props_type::ctx_type;
    using intf_type = function_request_intf<ctx_type, value_type>;
    using impl_type = function_request_impl<
        value_type,
        intf_type,
        props_type::func_is_coro,
        decltype(&func_a)>;

    auto good_impl{std::make_shared<impl_type>(good_uuid, func_a)};
    std::stringstream os;
    {
        cereal::JSONOutputArchive oarchive(os);
        oarchive(good_impl);
    }
    std::string good_seri = os.str();

    auto bad_seri{std::regex_replace(
        good_seri, std::regex{good_uuid_str}, bad_uuid_str)};
    std::istringstream is(bad_seri);
    cereal::JSONInputArchive iarchive(is);
    std::shared_ptr<impl_type> bad_impl;
    REQUIRE_THROWS_AS(iarchive(bad_impl), no_function_for_uuid_error);
}
