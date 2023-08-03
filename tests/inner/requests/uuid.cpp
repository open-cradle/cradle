#include <cstring>

#include <catch2/catch.hpp>

#include <cradle/inner/requests/uuid.h>

using namespace cradle;

TEST_CASE("uuid_error ctor - string")
{
    uuid_error res{std::string("reason")};
    REQUIRE(strlen(res.what()) > 0);
}

TEST_CASE("uuid_error ctor - C-style")
{
    uuid_error res{"reason"};
    REQUIRE(strlen(res.what()) > 0);
}

TEST_CASE("request_uuid ctor - bad base", "[uuid]")
{
    REQUIRE_THROWS_WITH(
        request_uuid{"b+ase"},
        "Invalid character(s) in request_uuid base b+ase");
}

TEST_CASE("compare request_id's", "[uuid]")
{
    request_uuid x{"x"};
    request_uuid y{"y"};

    REQUIRE(x == x);
    REQUIRE(x != y);
    REQUIRE(x < y);
    REQUIRE(x <= x);
    REQUIRE(y > x);
    REQUIRE(y >= x);
}
