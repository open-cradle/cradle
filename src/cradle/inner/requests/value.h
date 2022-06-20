#ifndef CRADLE_INNER_REQUESTS_VALUE_H
#define CRADLE_INNER_REQUESTS_VALUE_H

#include <memory>
#include <utility>

#include <cppcoro/task.hpp>

#include <cradle/inner/requests/generic.h>

namespace cradle {

/**
 * Request for an immediate value. No caching, no introspection.
 */
template<typename Value>
class value_request
{
 public:
    using element_type = value_request;
    using value_type = Value;

    static constexpr caching_level_type caching_level{
        caching_level_type::none};

    value_request(Value const& value) : value_{value}
    {
    }

    value_request(Value&& value) : value_{std::move(value)}
    {
    }

    Value
    get_value() const
    {
        return value_;
    }

    // VS2019 build fails with
    // resolve(UncachedContext auto& ctx) const
    template<UncachedContext Ctx>
    cppcoro::task<Value>
    resolve(Ctx& ctx) const
    {
        co_return value_;
    }

 private:
    Value value_;
};

template<typename Value>
auto
rq_value(Value&& value)
{
    return value_request{std::forward<Value>(value)};
}

template<typename Value>
auto
rq_value_up(Value&& value)
{
    return std::make_unique<value_request<Value>>(std::forward<Value>(value));
}

template<typename Value>
auto
rq_value_sp(Value&& value)
{
    return std::make_shared<value_request<Value>>(std::forward<Value>(value));
}

} // namespace cradle

#endif
