#include <type_traits>
#include <typeinfo>
#include <utility>

#include <catch2/catch.hpp>

#include <cradle/inner/utilities/demangle.h>

using namespace cradle;

namespace {

char const tag[] = "[core][utilities][demangle]";

int
func_a(int x)
{
    return x;
}

int
func_b(int x)
{
    return x;
}

template<typename Function>
auto
make_lambda(Function function)
{
    if constexpr (std::is_function_v<std::remove_pointer_t<Function>>)
    {
        return [=](auto&&... args) {
            return function(std::forward<decltype(args)>(args)...);
        };
    }
    else
    {
        return function;
    }
}

} // namespace

TEST_CASE("demangling identical types gives identical results", tag)
{
    std::string d0{demangle(typeid(std::string))};
    std::string d1{demangle(typeid(std::string))};

    REQUIRE(d0 == d1);
}

TEST_CASE("demangling different types gives different results", tag)
{
    std::string d0{demangle(typeid(int))};
    std::string d1{demangle(typeid(double))};

    REQUIRE(d0 != d1);
}

// The ABI apparently cannot demangle these lambdas.
// The demangle wrapper in cereal would crash on them.
TEST_CASE("recover from failing demangle", tag)
{
    auto lambda0{make_lambda(func_a)};
    auto lambda1{make_lambda(func_b)};
    std::string d0{demangle(typeid(lambda0))};
    std::string d1{demangle(typeid(lambda1))};

    REQUIRE(d0 == d1);
}
