#include <memory>
#include <stdexcept>

#include <catch2/catch.hpp>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/remote/proxy.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][remote]";

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

    serialized_result
    resolve_sync(
        remote_context_intf& ctx,
        std::string domain_name,
        std::string seri_req) override
    {
        throw not_implemented_error{"test_proxy::resolve_sync()"};
    }

    async_id
    submit_async(
        remote_context_intf& ctx,
        std::string domain_name,
        std::string seri_req) override
    {
        throw not_implemented_error{"test_proxy::submit_async()"};
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

 private:
    std::string name_;
};

} // namespace

TEST_CASE("construct remote_error, one arg", tag)
{
    auto exc{remote_error("argX")};

    std::string what(exc.what());

    REQUIRE(what.find("argX") != std::string::npos);
}

TEST_CASE("construct remote_error, two args", tag)
{
    auto exc{remote_error("argX", "argY")};

    std::string what(exc.what());

    REQUIRE(what.find("argX") != std::string::npos);
    REQUIRE(what.find("argY") != std::string::npos);
}

TEST_CASE("register and find proxy", tag)
{
    auto a_proxy = std::make_shared<test_proxy>("a");
    auto b_proxy = std::make_shared<test_proxy>("b");
    register_proxy(a_proxy);
    register_proxy(b_proxy);

    REQUIRE(&find_proxy("b") == &*b_proxy);
    REQUIRE(&find_proxy("a") == &*a_proxy);
}

TEST_CASE("find unregistered proxy", tag)
{
    REQUIRE_THROWS_AS(find_proxy("nonesuch"), std::logic_error);
}
