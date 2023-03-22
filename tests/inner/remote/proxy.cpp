#include <memory>
#include <stdexcept>

#include <catch2/catch.hpp>

#include <cradle/inner/remote/proxy.h>

using namespace cradle;

class test_proxy : public remote_proxy
{
 public:
    test_proxy(std::string name) : name_{std::move(name)} {};

    std::string
    name() const override
    {
        return name_;
    }

    cppcoro::task<serialized_result>
    resolve_request(remote_context_intf& ctx, std::string seri_req) override
    {
        co_return serialized_result(blob());
    }

 private:
    std::string name_;
};

TEST_CASE("register and find proxy", "[inner][remote]")
{
    auto a_proxy = std::make_shared<test_proxy>("a");
    auto b_proxy = std::make_shared<test_proxy>("b");
    register_proxy(a_proxy);
    register_proxy(b_proxy);

    REQUIRE(find_proxy("b") == b_proxy);
    REQUIRE(find_proxy("a") == a_proxy);
}

TEST_CASE("find unregistered proxy", "[inner][remote]")
{
    REQUIRE_THROWS_AS(find_proxy("nonesuch"), std::logic_error);
}
