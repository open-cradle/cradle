#include <memory>
#include <stdexcept>

#include <catch2/catch.hpp>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/remote/proxy.h>

#include "../../support/inner_service.h"

using namespace cradle;

namespace {

static char const tag[] = "[inner][service][resources]";

class test_proxy : public remote_proxy
{
 public:
    test_proxy(std::string name) : name_{std::move(name)} {};

    std::string
    name() const override
    {
        return name_;
    }

    spdlog::logger&
    get_logger() override
    {
        throw not_implemented_error{"test_proxy::get_logger()"};
    }

    void
    store_request(
        std::string storage_name,
        std::string key,
        std::string seri_req) override
    {
        throw not_implemented_error{"test_proxy::store_request()"};
    }

    serialized_result
    resolve_sync(service_config config, std::string seri_req) override
    {
        throw not_implemented_error{"test_proxy::resolve_sync()"};
    }

    async_id
    submit_async(service_config config, std::string seri_req) override
    {
        throw not_implemented_error{"test_proxy::submit_async()"};
    }

    async_id
    submit_stored(
        service_config config,
        std::string storage_name,
        std::string key) override
    {
        throw not_implemented_error{"test_proxy::submit_stored()"};
    }

    remote_context_spec_list
    get_sub_contexts(async_id aid) override
    {
        throw not_implemented_error{"test_proxy::get_sub_contexts()"};
    }

    async_status
    get_async_status(async_id aid) override
    {
        throw not_implemented_error{"test_proxy::get_async_status()"};
    }

    std::string
    get_async_error_message(async_id aid) override
    {
        throw not_implemented_error{"test_proxy::get_async_error_message()"};
    }

    serialized_result
    get_async_response(async_id root_aid) override
    {
        throw not_implemented_error{"test_proxy::get_async_response()"};
    }

    request_essentials
    get_essentials(async_id aid) override
    {
        throw not_implemented_error{"test_proxy::get_essentials()"};
    }

    void
    request_cancellation(async_id aid) override
    {
        throw not_implemented_error{"test_proxy::request_cancellation()"};
    }

    void
    finish_async(async_id root_aid) override
    {
        throw not_implemented_error{"test_proxy::finish_async()"};
    }

    tasklet_info_list
    get_tasklet_infos(bool include_finished) override
    {
        throw not_implemented_error{"test_proxy::get_tasklet_infos()"};
    }

    void
    load_shared_library(std::string dir_path, std::string dll_name) override
    {
        throw not_implemented_error("test_proxy::load_shared_library()");
    }

    void
    unload_shared_library(std::string dll_name) override
    {
        throw not_implemented_error("test_proxy::unload_shared_library()");
    }

    void
    mock_http(std::string const& response_body) override
    {
        throw not_implemented_error("test_proxy::mock_http()");
    }

    void
    clear_unused_mem_cache_entries() override
    {
        throw not_implemented_error(
            "test_proxy::clear_unused_mem_cache_entries()");
    }

    void
    release_cache_record_lock(remote_cache_record_id record_id) override
    {
        throw not_implemented_error("test_proxy::release_cache_record_lock()");
    }

    int
    get_num_contained_calls() const override
    {
        throw not_implemented_error("test_proxy::get_num_contained_calls()");
    }

 private:
    std::string name_;
};

} // namespace

TEST_CASE("register and find proxy", tag)
{
    auto resources{make_inner_test_resources()};

    auto a_proxy = std::make_unique<test_proxy>("a");
    auto b_proxy = std::make_unique<test_proxy>("b");
    auto* a_ptr = &*a_proxy;
    auto* b_ptr = &*b_proxy;

    resources->register_proxy(std::move(a_proxy));
    resources->register_proxy(std::move(b_proxy));

    REQUIRE(&resources->get_proxy("b") == b_ptr);
    REQUIRE(&resources->get_proxy("a") == a_ptr);
}

TEST_CASE("re-register proxy", tag)
{
    auto resources{make_inner_test_resources()};

    auto a0_proxy = std::make_unique<test_proxy>("a");
    auto a1_proxy = std::make_unique<test_proxy>("a");

    resources->register_proxy(std::move(a0_proxy));
    REQUIRE_THROWS_WITH(
        resources->register_proxy(std::move(a1_proxy)),
        "Proxy a already registered");
}

TEST_CASE("get unregistered proxy", tag)
{
    auto resources{make_inner_test_resources()};
    auto a_proxy = std::make_unique<test_proxy>("a");
    resources->register_proxy(std::move(a_proxy));

    REQUIRE_THROWS_WITH(
        resources->get_proxy("nonesuch"), "Proxy nonesuch not registered");
}
