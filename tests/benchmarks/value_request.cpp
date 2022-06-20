#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/requests/value.h>
#include <cradle/inner/service/core.h>

#include "../inner/support/core.h"
#include "support.h"

using namespace cradle;

TEST_CASE("value request", "[value]")
{
    using Value = int;

    BENCHMARK("create request")
    {
        return cradle::rq_value(Value(42));
    };

    BENCHMARK("1000x resolve()")
    {
        call_resolve_by_ref_loop(rq_value(42), 42);
    };

    BENCHMARK("1000x resolve_request")
    {
        resolve_request_loop(rq_value(42), 42);
    };
}

TEST_CASE("value request in unique_ptr", "[value]")
{
    using Value = int;

    BENCHMARK("create request")
    {
        return cradle::rq_value_up(Value(42));
    };

    BENCHMARK("1000x resolve()")
    {
        call_resolve_by_ptr_loop(rq_value_up(42), 42);
    };

    BENCHMARK("1000x resolve_request")
    {
        resolve_request_loop(rq_value_up(42), 42);
    };
}

TEST_CASE("value request in shared_ptr", "[value]")
{
    using Value = int;

    BENCHMARK("create request")
    {
        return cradle::rq_value_sp(Value(42));
    };

    BENCHMARK("1000x resolve()")
    {
        call_resolve_by_ptr_loop(rq_value_sp(42), 42);
    };

    BENCHMARK("1000x resolve_request")
    {
        resolve_request_loop(rq_value_sp(42), 42);
    };
}
