#include <cradle/inner/requests/function.h>

#include <optional>
#include <string>

#include <catch2/catch.hpp>

#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/requests/uuid.h>

using namespace cradle;

static char const tag[] = "[unit][inner][pool][request_pool_name]";

namespace {

int
add(int a, int b)
{
    return a + b;
}

// Builds a request that is identical across calls (same uuid and args), so any
// difference in identity must come from a runtime attribute such as the pool
// name rather than from construction.
auto
make_request()
{
    request_props<caching_level_type::memory> props{
        request_uuid{"unit-pool-request"}};
    return rq_function(props, add, 2, 3);
}

template<typename Value, typename Props>
std::string
unique_hash_string(function_request<Value, Props> const& req)
{
    unique_hasher hasher;
    req.update_hash(hasher);
    return hasher.get_string();
}

} // namespace

TEST_CASE("request pool name - accessor round-trip and default", tag)
{
    auto req{make_request()};

    REQUIRE(req.get_pool_name() == std::nullopt);

    req.set_pool_name(std::optional<std::string>{"some_pool"});
    REQUIRE(req.get_pool_name() == std::optional<std::string>{"some_pool"});

    req.set_pool_name(std::nullopt);
    REQUIRE(req.get_pool_name() == std::nullopt);
}

TEST_CASE("request pool name - identity-neutral for memory hash", tag)
{
    auto req_unset{make_request()};
    auto req_pool{make_request()};
    auto req_other{make_request()};

    req_pool.set_pool_name(std::optional<std::string>{"some_pool"});
    req_other.set_pool_name(std::optional<std::string>{"other_pool"});

    REQUIRE(req_pool.hash() == req_unset.hash());
    REQUIRE(req_other.hash() == req_unset.hash());
    REQUIRE(req_pool.hash() == req_other.hash());
}

TEST_CASE("request pool name - identity-neutral for unique hash", tag)
{
    auto req_unset{make_request()};
    auto req_pool{make_request()};
    auto req_other{make_request()};

    req_pool.set_pool_name(std::optional<std::string>{"some_pool"});
    req_other.set_pool_name(std::optional<std::string>{"other_pool"});

    std::string const hash_unset{unique_hash_string(req_unset)};
    std::string const hash_pool{unique_hash_string(req_pool)};
    std::string const hash_other{unique_hash_string(req_other)};

    REQUIRE(hash_pool == hash_unset);
    REQUIRE(hash_other == hash_unset);
    REQUIRE(hash_pool == hash_other);
}
