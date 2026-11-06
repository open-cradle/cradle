#include <string>
#include <type_traits>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include "../../support/inner_service.h"
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/resolve/resolve_request.h>

using namespace cradle;

namespace {

char const tag[] = "[unit][inner][requests][pool_plain_args]";

// Exercises detail::rebuild_pool_plain_arg on the is_function_request_v branch:
// the pool plain-args loader rebuilds a registered request-shaped Arg (Elem)
// from an already-resolved plain value on the worker. Verifies both that the
// reconstructed type equals the registered Arg type (the compile-time contract)
// and that it resolves to the original value at runtime, for the given retrier
// and value type.
template<typename Retrier, typename V>
void
check_rebuild_function_request_arg(V const& value)
{
    // The registered Arg shape (Elem) that reaches the branch via
    // normalize_arg passthrough: a coroutine function_request carrying Retrier.
    using props_t = request_props<
        caching_level_type::none,
        request_function_t::coro,
        false,
        Retrier>;
    auto arg{rq_function(
        props_t{request_uuid{"pool_plain_args/elem"}},
        identity_coro<V>,
        value)};
    using Elem = decltype(arg);
    static_assert(std::is_same_v<typename Elem::retrier_type, Retrier>);

    // Rebuild the Arg from a resolved plain value, as the worker does. ImplProps
    // is unused on this branch; Elem::props_type is a convenient placeholder.
    auto rebuilt{
        detail::rebuild_pool_plain_arg<typename Elem::props_type, Elem>(
            V{value})};
    static_assert(std::is_same_v<decltype(rebuilt), Elem>);

    auto resources{make_inner_test_resources()};
    non_caching_request_resolution_context ctx{*resources};
    auto resolved{cppcoro::sync_wait(resolve_request(ctx, rebuilt))};
    REQUIRE(resolved == value);
}

} // namespace

TEST_CASE(
    "pool plain-args rebuild of a default_retrier function_request arg", tag)
{
    // The newly-enabled case: the registered Arg's retrier is default_retrier
    // (as for Thinknode-style requests), across the value types in play.
    check_rebuild_function_request_arg<default_retrier>(std::string{"hello"});
    check_rebuild_function_request_arg<default_retrier>(make_blob("world"));
}

TEST_CASE(
    "pool plain-args rebuild of a no_retrier function_request arg", tag)
{
    // The pre-existing case must keep working after the retrier fix.
    check_rebuild_function_request_arg<no_retrier>(std::string{"hi"});
    check_rebuild_function_request_arg<no_retrier>(make_blob("there"));
}
