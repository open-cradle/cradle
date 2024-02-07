#include <cstring>

#include <catch2/catch.hpp>

#include <cradle/inner/requests/uuid.h>

using namespace cradle;

namespace {

static char const tag[] = "[uuid]";

}

TEST_CASE("uuid_error ctor - string", tag)
{
    uuid_error res{std::string("reason")};
    REQUIRE(strlen(res.what()) > 0);
}

TEST_CASE("uuid_error ctor - C-style", tag)
{
    uuid_error res{"reason"};
    REQUIRE(strlen(res.what()) > 0);
}

TEST_CASE("request_uuid ctor - good base", tag)
{
    request_uuid uuid{"base"};
    REQUIRE(uuid.str() == "base");
}

TEST_CASE("request_uuid ctor - bad base", tag)
{
    REQUIRE_THROWS_WITH(
        request_uuid{"b+ase"},
        "Invalid character(s) in request_uuid base b+ase");
}

TEST_CASE("compare request_id's", tag)
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

TEST_CASE("request_uuid - set caching level none", tag)
{
    auto uuid{request_uuid{"base"}.set_level(caching_level_type::none)};
    REQUIRE(uuid.str() == "base+none");
}

TEST_CASE("request_uuid - set caching level memory", tag)
{
    auto uuid{request_uuid{"base"}.set_level(caching_level_type::memory)};
    REQUIRE(uuid.str() == "base+mem");
}

TEST_CASE("request_uuid - set caching level full", tag)
{
    auto uuid{request_uuid{"base"}.set_level(caching_level_type::full)};
    REQUIRE(uuid.str() == "base+full");
}

TEST_CASE("request_uuid - set flattened", tag)
{
    auto uuid{request_uuid{"base"}.set_flattened()};
    REQUIRE(uuid.str() == "base+flattened");
}

TEST_CASE("request_uuid - set already flattened", tag)
{
    auto uuid{request_uuid{"base"}.set_flattened()};
    REQUIRE_THROWS_WITH(
        uuid.set_flattened(), "request_uuid object already flattened");
}

TEST_CASE("request_uuid - set caching level and flattened", tag)
{
    auto uuid0{request_uuid{"base"}
                   .set_level(caching_level_type::full)
                   .set_flattened()};
    REQUIRE(uuid0.str() == "base+full+flattened");

    auto uuid1{request_uuid{"base"}.set_flattened().set_level(
        caching_level_type::full)};
    REQUIRE(uuid1.str() == "base+full+flattened");
}

TEST_CASE("request_uuid - clone base unfinalized", tag)
{
    auto uuid{request_uuid{"base"}};
    auto clone{uuid.clone()};
    REQUIRE(clone.str() == uuid.str());
}

TEST_CASE("request_uuid - clone base finalized", tag)
{
    auto uuid{request_uuid{"base"}};
    uuid.str();
    auto clone{uuid.clone()};
    REQUIRE(clone.str() == uuid.str());
}

TEST_CASE("request_uuid - clone extended unfinalized", tag)
{
    auto uuid{request_uuid{"base"}
                  .set_level(caching_level_type::full)
                  .set_flattened()};
    auto clone{uuid.clone()};
    REQUIRE(clone.str() == uuid.str());
}

TEST_CASE("request_uuid - clone extended finalized", tag)
{
    auto uuid{request_uuid{"base"}
                  .set_level(caching_level_type::full)
                  .set_flattened()};
    uuid.str();
    auto clone{uuid.clone()};
    REQUIRE(clone.str() == uuid.str());
}
