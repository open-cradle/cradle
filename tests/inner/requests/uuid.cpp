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

TEST_CASE("request_uuid ctor - default", "[uuid]")
{
    request_uuid res;
    REQUIRE(res.str() == "");
    REQUIRE(!res.is_real());
}

TEST_CASE("request_uuid ctor - Git version", "[uuid]")
{
    request_uuid res{"base"};
    REQUIRE(res.str().starts_with("base+"));
    REQUIRE(res.is_real());
}

TEST_CASE("request_uuid ctor - explicit version", "[uuid]")
{
    request_uuid res{"base", "vers"};
    REQUIRE(res.str() == "base+vers");
    REQUIRE(res.is_real());
}

TEST_CASE("request_uuid ctor - bad base", "[uuid]")
{
    auto matcher = Catch::StartsWith("Invalid character(s) in base uuid ");
    REQUIRE_THROWS_WITH(new request_uuid("b+ase"), matcher);
    REQUIRE_THROWS_WITH(new request_uuid("b/ase"), matcher);
    REQUIRE_THROWS_WITH(new request_uuid("b+ase", "vers"), matcher);
    REQUIRE_THROWS_WITH(new request_uuid("b/ase", "vers"), matcher);
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

TEST_CASE("combined_uuid - normal", "[uuid]")
{
    request_uuid main_uuid{"mbase", "mver"};
    request_uuid sub_uuid{"sbase", "sver"};

    auto res = combined_uuid(main_uuid, sub_uuid);

    REQUIRE(res.str() == "mbase+mver/sbase+sver");
    REQUIRE(res.is_real());
}

TEST_CASE("combined_uuid - no sub", "[uuid]")
{
    request_uuid main_uuid{"mbase", "mver"};
    request_uuid sub_uuid;

    auto res = combined_uuid(main_uuid, sub_uuid);

    REQUIRE(res.str() == "mbase+mver");
    REQUIRE(res.is_real());
}
