#ifndef CRADLE_WEBSOCKET_CALCULATIONS_H
#define CRADLE_WEBSOCKET_CALCULATIONS_H

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/thinknode/context.h>
#include <cradle/typing/service/core.h>
#include <cradle/websocket/types.hpp>

namespace cradle {

template<class Return, class... Args>
auto
make_function_id(Return (*ptr)(Args... args))
{
    return make_captured_id(ptr);
}

// Make an ID for a lambda function (or potentially another function object).
//
// This relies on two facts: a) Lambda functions are implemented as
// compiler-generated function objects with unique types, and b) the CRADLE ID
// system distinguishes IDs based on type. Thus, simply including the type of
// the lambda function in the ID type produces a unique ID that's specific to
// the lambda function.
//
// Note that we assume here that the lambda doesn't capture any variables that
// would need to be accounted for in the ID. That's considered a usage error.
// It could probably be checked for in the future.
//
template<class Function>
auto
make_function_id(Function f)
{
    return make_captured_id(typeid(f).name());
}

template<class Function>
lambda_function
make_function(Function&& function)
{
    lambda_function f;
    f.id = make_function_id(function);
    f.object = std::forward<Function>(function);
    return f;
}

cppcoro::task<dynamic>
resolve_calc_to_value(
    thinknode_request_context ctx,
    string context_id,
    calculation_request request);

cppcoro::task<std::string>
resolve_calc_to_iss_object(
    thinknode_request_context ctx,
    string context_id,
    calculation_request request);

calculation_request
as_generic_calc(thinknode_calc_request const& request);

} // namespace cradle

#endif
