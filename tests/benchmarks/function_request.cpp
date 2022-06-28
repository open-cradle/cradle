#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/service/core.h>

#include "../inner/support/core.h"
#include "support.h"

using namespace cradle;

static auto add = [](int a, int b) { return a + b; };

template<int H>
auto
create_thin_tree()
{
    auto constexpr level = caching_level_type::none;
    return rq_function<level>(add, create_thin_tree<H - 1>(), rq_value(1));
}

template<>
auto
create_thin_tree<1>()
{
    auto constexpr level = caching_level_type::none;
    return rq_function<level>(add, rq_value(2), rq_value(1));
}

template<int H>
auto
create_triangular_tree()
{
    auto constexpr level = caching_level_type::none;
    return rq_function<level>(
        add, create_triangular_tree<H - 1>(), create_triangular_tree<H - 1>());
}

template<>
auto
create_triangular_tree<1>()
{
    auto constexpr level = caching_level_type::none;
    return rq_function<level>(add, rq_value(2), rq_value(1));
}

TEST_CASE("create function request", "[function]")
{
    BENCHMARK("create thin tree H=2")
    {
        return create_thin_tree<2>();
    };

    BENCHMARK("create thin tree H=4")
    {
        return create_thin_tree<4>();
    };

    BENCHMARK("create thin tree H=16")
    {
        return create_thin_tree<16>();
    };

    BENCHMARK("create thin tree H=64")
    {
        return create_thin_tree<64>();
    };

    BENCHMARK("create △ tree H=2")
    {
        return create_triangular_tree<2>();
    };

    BENCHMARK("create △ tree H=4")
    {
        return create_triangular_tree<4>();
    };

    BENCHMARK("create △ tree H=6")
    {
        return create_triangular_tree<6>();
    };
}

template<int H>
auto
resolve_loop_thin()
{
    auto expected = 2 + H;
    resolve_request_loop(create_thin_tree<H>(), expected);
}

template<int H>
auto
resolve_loop_triangular()
{
    int expected = (1 << (H - 1)) * 3;
    resolve_request_loop(create_triangular_tree<H>(), expected);
}

TEST_CASE("resolve function request", "[function]")
{

    BENCHMARK("1000x resolve thin tree H=2")
    {
        resolve_loop_thin<2>();
    };

    BENCHMARK("1000x resolve thin tree H=4")
    {
        resolve_loop_thin<4>();
    };

    BENCHMARK("1000x resolve thin tree H=16")
    {
        resolve_loop_thin<16>();
    };

#ifndef _MSC_VER
    // VS2019 internal compiler error
    BENCHMARK("1000x resolve thin tree H=64")
    {
        resolve_loop_thin<64>();
    };
#endif

    BENCHMARK("1000x resolve △ tree H=2")
    {
        return resolve_loop_triangular<2>();
    };

    BENCHMARK("1000x resolve △ tree H=4")
    {
        return resolve_loop_triangular<4>();
    };

    BENCHMARK("1000x resolve △ tree H=6")
    {
        return resolve_loop_triangular<6>();
    };
}

template<int H>
auto
create_thin_tree_up()
{
    auto constexpr level = caching_level_type::none;
    return rq_function_up<level>(
        add, create_thin_tree_up<H - 1>(), rq_value_up(1));
}

template<>
auto
create_thin_tree_up<1>()
{
    auto constexpr level = caching_level_type::none;
    return rq_function_up<level>(add, rq_value_up(2), rq_value_up(1));
}

template<int H>
auto
create_triangular_tree_up()
{
    auto constexpr level = caching_level_type::none;
    return rq_function_up<level>(
        add,
        create_triangular_tree_up<H - 1>(),
        create_triangular_tree_up<H - 1>());
}

template<>
auto
create_triangular_tree_up<1>()
{
    auto constexpr level = caching_level_type::none;
    return rq_function_up<level>(add, rq_value_up(2), rq_value_up(1));
}

TEST_CASE("create function request in unique_ptr", "[function]")
{
    BENCHMARK("create thin tree H=2")
    {
        return create_thin_tree_up<2>();
    };

    BENCHMARK("create thin tree H=4")
    {
        return create_thin_tree_up<4>();
    };

    // Taller trees tend to blow up the compilers.

    BENCHMARK("create △ tree H=2")
    {
        return create_triangular_tree_up<2>();
    };

    BENCHMARK("create △ tree H=4")
    {
        return create_triangular_tree_up<4>();
    };

    BENCHMARK("create △ tree H=6")
    {
        return create_triangular_tree_up<6>();
    };
}

template<int H>
auto
resolve_loop_thin_up()
{
    auto expected = 2 + H;
    resolve_request_loop(create_thin_tree_up<H>(), expected);
}

template<int H>
auto
resolve_loop_triangular_up()
{
    int expected = (1 << (H - 1)) * 3;
    resolve_request_loop(create_triangular_tree_up<H>(), expected);
}

TEST_CASE("resolve function request in unique_ptr", "[function]")
{

    BENCHMARK("1000x resolve thin tree H=2")
    {
        resolve_loop_thin_up<2>();
    };

    BENCHMARK("1000x resolve thin tree H=4")
    {
        resolve_loop_thin_up<4>();
    };

    // Taller trees tend to blow up the compilers.

    BENCHMARK("1000x resolve △ tree H=2")
    {
        return resolve_loop_triangular_up<2>();
    };

    BENCHMARK("1000x resolve △ tree H=4")
    {
        return resolve_loop_triangular_up<4>();
    };

    BENCHMARK("1000x resolve △ tree H=6")
    {
        return resolve_loop_triangular_up<6>();
    };
}

template<int H>
auto
create_thin_tree_sp()
{
    auto constexpr level = caching_level_type::none;
    return rq_function_sp<level>(
        add, create_thin_tree_sp<H - 1>(), rq_value_sp(1));
}

template<>
auto
create_thin_tree_sp<1>()
{
    auto constexpr level = caching_level_type::none;
    return rq_function_sp<level>(add, rq_value_sp(2), rq_value_sp(1));
}

template<int H>
auto
create_triangular_tree_sp()
{
    auto constexpr level = caching_level_type::none;
    return rq_function_sp<level>(
        add,
        create_triangular_tree_sp<H - 1>(),
        create_triangular_tree_sp<H - 1>());
}

template<>
auto
create_triangular_tree_sp<1>()
{
    auto constexpr level = caching_level_type::none;
    return rq_function_sp<level>(add, rq_value_sp(2), rq_value_sp(1));
}

TEST_CASE("create function request in shared_ptr", "[function]")
{
    BENCHMARK("create thin tree H=2")
    {
        return create_thin_tree_sp<2>();
    };

    BENCHMARK("create thin tree H=4")
    {
        return create_thin_tree_sp<4>();
    };

    BENCHMARK("create thin tree H=16")
    {
        return create_thin_tree_sp<16>();
    };

    BENCHMARK("create thin tree H=64")
    {
        return create_thin_tree_sp<64>();
    };

    BENCHMARK("create △ tree H=2")
    {
        return create_triangular_tree_sp<2>();
    };

    BENCHMARK("create △ tree H=4")
    {
        return create_triangular_tree_sp<4>();
    };

    BENCHMARK("create △ tree H=6")
    {
        return create_triangular_tree_sp<6>();
    };
}

template<int H>
auto
resolve_loop_thin_sp()
{
    auto expected = 2 + H;
    resolve_request_loop(create_thin_tree_sp<H>(), expected);
}

template<int H>
auto
resolve_loop_triangular_sp()
{
    int expected = (1 << (H - 1)) * 3;
    resolve_request_loop(create_triangular_tree_sp<H>(), expected);
}

TEST_CASE("resolve function request in shared_ptr", "[function]")
{

    BENCHMARK("1000x resolve thin tree H=2")
    {
        resolve_loop_thin_sp<2>();
    };

    BENCHMARK("1000x resolve thin tree H=4")
    {
        resolve_loop_thin_sp<4>();
    };

    BENCHMARK("1000x resolve thin tree H=16")
    {
        resolve_loop_thin_sp<16>();
    };

    BENCHMARK("1000x resolve thin tree H=64")
    {
        resolve_loop_thin_sp<64>();
    };

    BENCHMARK("1000x resolve △ tree H=2")
    {
        return resolve_loop_triangular_sp<2>();
    };

    BENCHMARK("1000x resolve △ tree H=4")
    {
        return resolve_loop_triangular_sp<4>();
    };

    BENCHMARK("1000x resolve △ tree H=6")
    {
        return resolve_loop_triangular_sp<6>();
    };
}

template<int H>
auto
create_thin_tree_mixed()
{
    auto constexpr level = caching_level_type::none;
    return rq_function_sp<level>(
        add, create_thin_tree_mixed<H - 1>(), rq_value(1));
}

template<>
auto
create_thin_tree_mixed<1>()
{
    auto constexpr level = caching_level_type::none;
    return rq_function<level>(add, rq_value(2), rq_value(1));
}

template<>
auto
create_thin_tree_mixed<2>()
{
    auto constexpr level = caching_level_type::none;
    return rq_function<level>(add, create_thin_tree_mixed<1>(), rq_value(1));
}

template<int H>
auto
create_triangular_tree_mixed()
{
    auto constexpr level = caching_level_type::none;
    auto subreq{create_triangular_tree_mixed<H - 1>()};
    return rq_function_sp<level>(add, subreq, subreq);
}

template<>
auto
create_triangular_tree_mixed<1>()
{
    auto constexpr level = caching_level_type::none;
    return rq_function<level>(add, rq_value(2), rq_value(1));
}

template<>
auto
create_triangular_tree_mixed<2>()
{
    auto constexpr level = caching_level_type::none;
    return rq_function<level>(
        add,
        create_triangular_tree_mixed<1>(),
        create_triangular_tree_mixed<1>());
}

TEST_CASE("create mixed function request", "[function]")
{
    BENCHMARK("create thin tree H=2")
    {
        return create_thin_tree_mixed<2>();
    };

    BENCHMARK("create thin tree H=4")
    {
        return create_thin_tree_mixed<4>();
    };

    BENCHMARK("create thin tree H=16")
    {
        return create_thin_tree_mixed<16>();
    };

    BENCHMARK("create thin tree H=64")
    {
        return create_thin_tree_mixed<64>();
    };

    BENCHMARK("create △ tree H=2")
    {
        return create_triangular_tree_mixed<2>();
    };

    BENCHMARK("create △ tree H=4")
    {
        return create_triangular_tree_mixed<4>();
    };

    BENCHMARK("create △ tree H=6")
    {
        return create_triangular_tree_mixed<6>();
    };
}

template<int H>
auto
resolve_loop_thin_mixed()
{
    auto expected = 2 + H;
    resolve_request_loop(create_thin_tree_mixed<H>(), expected);
}

template<int H>
auto
resolve_loop_triangular_mixed()
{
    int expected = (1 << (H - 1)) * 3;
    resolve_request_loop(create_triangular_tree_mixed<H>(), expected);
}

TEST_CASE("resolve mixed function request", "[function]")
{

    BENCHMARK("1000x resolve thin tree H=2")
    {
        resolve_loop_thin_mixed<2>();
    };

    BENCHMARK("1000x resolve thin tree H=4")
    {
        resolve_loop_thin_mixed<4>();
    };

    BENCHMARK("1000x resolve thin tree H=16")
    {
        resolve_loop_thin_mixed<16>();
    };

    BENCHMARK("1000x resolve thin tree H=64")
    {
        resolve_loop_thin_mixed<64>();
    };

    BENCHMARK("1000x resolve △ tree H=2")
    {
        return resolve_loop_triangular_mixed<2>();
    };

    BENCHMARK("1000x resolve △ tree H=4")
    {
        return resolve_loop_triangular_mixed<4>();
    };

    BENCHMARK("1000x resolve △ tree H=6")
    {
        return resolve_loop_triangular_mixed<6>();
    };
}

template<caching_level_type level, int H>
requires(H == 1) auto create_thin_tree_erased()
{
    return rq_function_erased<level>(add, rq_value(2), rq_value(1));
}

template<caching_level_type level, int H>
requires(H > 1) auto create_thin_tree_erased()
{
    return rq_function_erased<level>(
        add, create_thin_tree_erased<level, H - 1>(), rq_value(1));
}

template<caching_level_type level, int H>
requires(H == 1) auto create_triangular_tree_erased()
{
    return rq_function_erased<level>(add, rq_value(2), rq_value(1));
}

template<caching_level_type level, int H>
requires(H > 1) auto create_triangular_tree_erased()
{
    return rq_function_erased<level>(
        add,
        create_triangular_tree_erased<level, H - 1>(),
        create_triangular_tree_erased<level, H - 1>());
}

template<caching_level_type level, int H>
requires(H == 1) auto create_triangular_tree_erased_introspected()
{
    std::string title{"add 2+1"};
    return rq_function_erased_intrsp<level>(
        title, add, rq_value(2), rq_value(1));
}

template<caching_level_type level, int H>
requires(H > 1) auto create_triangular_tree_erased_introspected()
{
    std::stringstream ss;
    ss << "add H" << H;
    std::string title{ss.str()};
    return rq_function_erased_intrsp<level>(
        title,
        add,
        create_triangular_tree_erased_introspected<level, H - 1>(),
        create_triangular_tree_erased_introspected<level, H - 1>());
}

TEST_CASE("create type-erased function request (uncached)", "[erased]")
{
    auto constexpr level = caching_level_type::none;

    BENCHMARK("create thin tree H=2")
    {
        return create_thin_tree_erased<level, 2>();
    };

    BENCHMARK("create thin tree H=4")
    {
        return create_thin_tree_erased<level, 4>();
    };

    BENCHMARK("create thin tree H=16")
    {
        return create_thin_tree_erased<level, 16>();
    };

    BENCHMARK("create thin tree H=64")
    {
        return create_thin_tree_erased<level, 64>();
    };

    BENCHMARK("create △ tree H=2")
    {
        return create_triangular_tree_erased<level, 2>();
    };

    BENCHMARK("create △ tree H=4")
    {
        return create_triangular_tree_erased<level, 4>();
    };

    BENCHMARK("create △ tree H=6")
    {
        return create_triangular_tree_erased<level, 6>();
    };
}

TEST_CASE("create type-erased function request (cached)", "[erased]")
{
    auto constexpr level = caching_level_type::memory;

    BENCHMARK("create thin tree H=2")
    {
        return create_thin_tree_erased<level, 2>();
    };

    BENCHMARK("create thin tree H=4")
    {
        return create_thin_tree_erased<level, 4>();
    };

    BENCHMARK("create thin tree H=16")
    {
        return create_thin_tree_erased<level, 16>();
    };

    BENCHMARK("create thin tree H=64")
    {
        return create_thin_tree_erased<level, 64>();
    };

    BENCHMARK("create △ tree H=2")
    {
        return create_triangular_tree_erased<level, 2>();
    };

    BENCHMARK("create △ tree H=4")
    {
        return create_triangular_tree_erased<level, 4>();
    };

    BENCHMARK("create △ tree H=6")
    {
        return create_triangular_tree_erased<level, 6>();
    };
}

TEST_CASE(
    "create type-erased function request (cached, introspected)", "[erased]")
{
    auto constexpr level = caching_level_type::memory;

    BENCHMARK("create △ tree H=4")
    {
        return create_triangular_tree_erased_introspected<level, 4>();
    };

    BENCHMARK("create △ tree H=6")
    {
        return create_triangular_tree_erased_introspected<level, 6>();
    };
}

template<caching_level_type level, int H>
auto
resolve_loop_thin_erased()
{
    auto expected = 2 + H;
    resolve_request_loop(create_thin_tree_erased<level, H>(), expected);
}

template<caching_level_type level, int H>
auto
resolve_loop_triangular_erased()
{
    int expected = (1 << (H - 1)) * 3;
    resolve_request_loop(create_triangular_tree_erased<level, H>(), expected);
    return expected;
}

TEST_CASE("resolve type-erased function request (uncached)", "[erased]")
{
    auto constexpr level = caching_level_type::none;

    BENCHMARK("1000x resolve thin tree H=2")
    {
        resolve_loop_thin_erased<level, 2>();
    };

    BENCHMARK("1000x resolve thin tree H=4")
    {
        resolve_loop_thin_erased<level, 4>();
    };

    BENCHMARK("1000x resolve thin tree H=16")
    {
        resolve_loop_thin_erased<level, 16>();
    };

    BENCHMARK("1000x resolve thin tree H=64")
    {
        resolve_loop_thin_erased<level, 64>();
    };

    BENCHMARK("1000x resolve △ tree H=2")
    {
        return resolve_loop_triangular_erased<level, 2>();
    };

    BENCHMARK("1000x resolve △ tree H=4")
    {
        return resolve_loop_triangular_erased<level, 4>();
    };

    BENCHMARK("1000x resolve △ tree H=6")
    {
        return resolve_loop_triangular_erased<level, 6>();
    };
}

TEST_CASE("resolve type-erased function request (memory-cached)", "[erased]")
{
    auto constexpr level = caching_level_type::memory;

    BENCHMARK("1000x resolve thin tree H=2")
    {
        resolve_loop_thin_erased<level, 2>();
    };

    BENCHMARK("1000x resolve thin tree H=4")
    {
        resolve_loop_thin_erased<level, 4>();
    };

    BENCHMARK("1000x resolve thin tree H=16")
    {
        resolve_loop_thin_erased<level, 16>();
    };

    BENCHMARK("1000x resolve thin tree H=64")
    {
        resolve_loop_thin_erased<level, 64>();
    };

    BENCHMARK("1000x resolve △ tree H=2")
    {
        return resolve_loop_triangular_erased<level, 2>();
    };

    BENCHMARK("1000x resolve △ tree H=4")
    {
        return resolve_loop_triangular_erased<level, 4>();
    };

    BENCHMARK("1000x resolve △ tree H=6")
    {
        return resolve_loop_triangular_erased<level, 6>();
    };
}

template<int H>
auto
resolve_loop_triangular_erased_full()
{
    auto constexpr level = caching_level_type::full;
    int expected = (1 << (H - 1)) * 3;
    resolve_request_loop_full(
        create_triangular_tree_erased<level, H>(), expected);
    return expected;
}

TEST_CASE("resolve type-erased function request (fully cached)", "[erased]")
{
    spdlog::set_level(spdlog::level::warn);

    BENCHMARK("1000x resolve △ tree H=4")
    {
        return resolve_loop_triangular_erased_full<4>();
    };

    BENCHMARK("1000x resolve △ tree H=6")
    {
        return resolve_loop_triangular_erased_full<6>();
    };
}
