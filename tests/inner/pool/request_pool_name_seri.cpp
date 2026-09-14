#include <optional>
#include <sstream>
#include <string>

#include <catch2/catch.hpp>
#include <cppcoro/task.hpp>
#include <fmt/format.h>

#include "../../support/inner_service.h"
#include <cradle/inner/encodings/msgpack_value.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/service/resources.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][pool][request_pool_name]";

cppcoro::task<std::string>
coro_value(context_intf& ctx)
{
    co_return "value";
}

request_uuid
make_test_uuid(std::string const& ext)
{
    return request_uuid{fmt::format("{}-{}", tag, ext)};
}

template<Request Req>
std::string
to_json(Req const& req)
{
    std::stringstream os;
    {
        JSONRequestOutputArchive oarchive(os);
        req.save(oarchive);
    }
    return os.str();
}

template<Request Req>
void
from_json(Req& req, std::string const& json, inner_resources& resources)
{
    std::istringstream is(json);
    JSONRequestInputArchive iarchive(is, resources);
    req.load(iarchive);
}

} // namespace

TEST_CASE("request pool name survives JSON round-trip", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::none, request_function_t::coro> props{
        make_test_uuid("json-set")};
    auto saved_req{rq_function(props, coro_value)};
    auto registry{resources->get_seri_registry()};
    seri_catalog cat{registry};
    cat.register_resolver(saved_req);

    saved_req.set_pool_name(std::optional<std::string>{"some_pool"});

    auto json = to_json(saved_req);
    decltype(saved_req) loaded_req;
    from_json(loaded_req, json, *resources);

    REQUIRE(
        loaded_req.get_pool_name() == std::optional<std::string>{"some_pool"});
}

TEST_CASE("request without a pool name round-trips as nullopt (JSON)", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::none, request_function_t::coro> props{
        make_test_uuid("json-unset")};
    auto saved_req{rq_function(props, coro_value)};
    auto registry{resources->get_seri_registry()};
    seri_catalog cat{registry};
    cat.register_resolver(saved_req);

    // No pool name set.
    auto json = to_json(saved_req);
    decltype(saved_req) loaded_req;
    from_json(loaded_req, json, *resources);

    REQUIRE(loaded_req.get_pool_name() == std::nullopt);
}

TEST_CASE("request pool name survives msgpack round-trip", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::none, request_function_t::coro> props{
        make_test_uuid("msgpack-set")};
    auto saved_req{rq_function(props, coro_value)};
    auto registry{resources->get_seri_registry()};
    seri_catalog cat{registry};
    cat.register_resolver(saved_req);

    saved_req.set_pool_name(std::optional<std::string>{"some_pool"});

    bool allow_blob_files{false};
    auto seri = serialize_value(saved_req, allow_blob_files);
    auto loaded_req = deserialize_value<decltype(saved_req)>(seri);

    REQUIRE(
        loaded_req.get_pool_name() == std::optional<std::string>{"some_pool"});
}

TEST_CASE("request without a pool name round-trips as nullopt (msgpack)", tag)
{
    auto resources{make_inner_test_resources()};
    request_props<caching_level_type::none, request_function_t::coro> props{
        make_test_uuid("msgpack-unset")};
    auto saved_req{rq_function(props, coro_value)};
    auto registry{resources->get_seri_registry()};
    seri_catalog cat{registry};
    cat.register_resolver(saved_req);

    // No pool name set.
    bool allow_blob_files{false};
    auto seri = serialize_value(saved_req, allow_blob_files);
    auto loaded_req = deserialize_value<decltype(saved_req)>(seri);

    REQUIRE(loaded_req.get_pool_name() == std::nullopt);
}
