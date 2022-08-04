#ifndef CRADLE_INNER_REQUESTS_VALUE_H
#define CRADLE_INNER_REQUESTS_VALUE_H

#include <concepts>
#include <memory>
#include <utility>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/unique_hash.h>
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

    void
    update_hash(unique_hasher& hasher) const
    {
        update_unique_hash(hasher, value_);
    }

    // VS2019 build fails with
    // resolve(UncachedContext auto& ctx) const
    template<UncachedContext Ctx>
    cppcoro::task<Value>
    resolve(Ctx& ctx) const
    {
        co_return value_;
    }

    template<typename Archive>
    void
    serialize(Archive& archive)
    {
        archive(value_);
    }

 private:
    Value value_;
};

template<typename Value>
struct arg_type_struct<value_request<Value>>
{
    using value_type = Value;
};

template<typename Value>
struct arg_type_struct<std::unique_ptr<value_request<Value>>>
{
    using value_type = Value;
};

template<typename Value>
struct arg_type_struct<std::shared_ptr<value_request<Value>>>
{
    using value_type = Value;
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

// For memory cache, ordered map
template<typename Value>
requires std::equality_comparable<Value> bool
operator==(value_request<Value> const& lhs, value_request<Value> const& rhs)
{
    return lhs.get_value() == rhs.get_value();
}

template<typename Value>
requires std::totally_ordered<Value> bool
operator<(value_request<Value> const& lhs, value_request<Value> const& rhs)
{
    return lhs.get_value() < rhs.get_value();
}

// For memory cache, unordered map
template<typename Value>
size_t
hash_value(value_request<Value> const& req)
{
    return invoke_hash(req.get_value());
}

template<typename Value>
size_t
hash_value(std::unique_ptr<value_request<Value>> const& req)
{
    return invoke_hash(req->get_value());
}

template<typename Value>
size_t
hash_value(std::shared_ptr<value_request<Value>> const& req)
{
    return invoke_hash(req->get_value());
}

template<typename Value>
void
update_unique_hash(unique_hasher& hasher, value_request<Value> const& req)
{
    req.update_hash(hasher);
}

template<typename Value>
void
update_unique_hash(
    unique_hasher& hasher, std::unique_ptr<value_request<Value>> const& req)
{
    req->update_hash(hasher);
}

template<typename Value>
void
update_unique_hash(
    unique_hasher& hasher, std::shared_ptr<value_request<Value>> const& req)
{
    req->update_hash(hasher);
}

} // namespace cradle

#endif
