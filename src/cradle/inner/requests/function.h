#ifndef CRADLE_INNER_REQUESTS_FUNCTION_H
#define CRADLE_INNER_REQUESTS_FUNCTION_H

#include <memory>
#include <utility>

#include <cppcoro/task.hpp>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/core.h>

namespace cradle {

template<caching_level_type level, class Function, class... Args>
class function_request
{
 public:
    using element_type = function_request;
    using value_type = std::
        invoke_result_t<Function, typename Args::element_type::value_type...>;

    static constexpr caching_level_type caching_level = level;

    function_request(Function function, Args... args)
        : function_{std::move(function)}, args_{std::move(args)...}
    {
    }

    template<typename Context>
    cppcoro::task<value_type>
    resolve(Context const& ctx) const
    {
        // The "func=function_" is a workaround to prevent a gcc-10 internal
        // compiler error in release builds.
        co_return co_await std::apply(
            [&, func = function_](auto&&... args)
                -> cppcoro::task<typename function_request::value_type> {
                co_return func((co_await resolve_request(ctx, args))...);
            },
            args_);
    }

 private:
    Function function_;
    std::tuple<Args...> args_;
};

template<caching_level_type level, class Function, class... Args>
auto
rq_function(Function function, Args... args)
{
    return function_request<level, Function, Args...>{
        std::move(function), std::move(args)...};
}

template<caching_level_type level, class Function, class... Args>
auto
rq_function_up(Function function, Args... args)
{
    return std::make_unique<function_request<level, Function, Args...>>(
        std::move(function), std::move(args)...);
}

template<caching_level_type level, class Function, class... Args>
auto
rq_function_sp(Function function, Args... args)
{
    return std::make_shared<function_request<level, Function, Args...>>(
        std::move(function), std::move(args)...);
}

} // namespace cradle

#endif
