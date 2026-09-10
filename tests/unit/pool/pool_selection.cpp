#include <cradle/inner/pool/pool_selection.h>

#include <optional>
#include <string>

#include <catch2/catch.hpp>

using namespace cradle;

static char const tag[] = "[unit][inner][pool][pool_selection]";

TEST_CASE("select_pool_name - request value wins over context default", tag)
{
    std::optional<std::string> const request_pool{"request_pool"};
    std::optional<std::string> const context_default{"context_pool"};

    REQUIRE(select_pool_name(request_pool, context_default) == request_pool);
}

TEST_CASE("select_pool_name - request value wins with no context default", tag)
{
    std::optional<std::string> const request_pool{"request_pool"};
    std::optional<std::string> const context_default{std::nullopt};

    REQUIRE(select_pool_name(request_pool, context_default) == request_pool);
}

TEST_CASE("select_pool_name - context default used when request is empty", tag)
{
    std::optional<std::string> const request_pool{std::nullopt};
    std::optional<std::string> const context_default{"context_pool"};

    REQUIRE(
        select_pool_name(request_pool, context_default) == context_default);
}

TEST_CASE("select_pool_name - neither present yields nullopt", tag)
{
    std::optional<std::string> const request_pool{std::nullopt};
    std::optional<std::string> const context_default{std::nullopt};

    REQUIRE(select_pool_name(request_pool, context_default) == std::nullopt);
}
