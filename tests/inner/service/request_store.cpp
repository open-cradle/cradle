#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>

#include "../../support/inner_service.h"
#include <cradle/inner/core/exception.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/service/request_store.h>
#include <cradle/plugins/secondary_cache/simple/simple_storage.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][service][request_store]";

request_uuid
make_test_uuid(int ext)
{
    return request_uuid{fmt::format("{}-{:04d}", tag, ext)};
}

static auto add2 = [](int a, int b) { return a + b; };

} // namespace

TEST_CASE("get_request_key()", tag)
{
    request_props<caching_level_type::full> props{make_test_uuid(100)};
    auto req0{rq_function(props, add2, 1, 2)};
    auto req1{rq_function(props, add2, 1, 3)};

    std::string key0{get_request_key(req0)};
    std::string key1{get_request_key(req1)};

    REQUIRE(key0 != "");
    REQUIRE(key0.size() == 64);
    REQUIRE(key0 != key1);
}

TEST_CASE("store request in storage", tag)
{
    inner_resources resources{make_inner_tests_config()};
    auto owned_storage{std::make_unique<simple_blob_storage>()};
    auto& storage{*owned_storage};
    resources.set_requests_storage(std::move(owned_storage));
    seri_catalog cat{resources.get_seri_registry()};
    request_props<caching_level_type::full> props{make_test_uuid(200)};

    auto req0{rq_function(props, add2, 1, 2)};
    cat.register_resolver(req0);
    cppcoro::sync_wait(store_request(req0, resources));

    REQUIRE(storage.size() == 1);

    auto req1{rq_function(props, add2, 1, 3)};
    cppcoro::sync_wait(store_request(req1, resources));

    REQUIRE(storage.size() == 2);
}

TEST_CASE("load request from storage (hit)", tag)
{
    inner_resources resources{make_inner_tests_config()};
    resources.set_requests_storage(std::make_unique<simple_blob_storage>());
    seri_catalog cat{resources.get_seri_registry()};

    request_props<caching_level_type::full> props{make_test_uuid(300)};
    auto req_written{rq_function(props, add2, 1, 2)};
    cat.register_resolver(req_written);
    cppcoro::sync_wait(store_request(req_written, resources));

    using Req = decltype(req_written);
    std::string key{get_request_key(req_written)};
    auto req_read = cppcoro::sync_wait(load_request<Req>(key, resources));
    REQUIRE(req_read == req_written);
}

TEST_CASE("load request from storage (miss)", tag)
{
    inner_resources resources{make_inner_tests_config()};
    resources.set_requests_storage(std::make_unique<simple_blob_storage>());
    seri_catalog cat{resources.get_seri_registry()};
    request_props<caching_level_type::full> props{make_test_uuid(400)};
    auto req_written{rq_function(props, add2, 1, 2)};
    cat.register_resolver(req_written);
    auto req_not_written{rq_function(props, add2, 1, 3)};
    cppcoro::sync_wait(store_request(req_written, resources));

    using Req = decltype(req_not_written);
    std::string key{get_request_key(req_not_written)};
    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(load_request<Req>(key, resources)),
        not_found_error);
}
