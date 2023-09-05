#ifndef CRADLE_INNER_REQUESTS_VALUE_H
#define CRADLE_INNER_REQUESTS_VALUE_H

#include <utility>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

/**
 * Request for an immediate value. No caching, no introspection.
 * Satisfies concept UncachedRequest.
 */
template<typename Value>
class value_request
{
 public:
    using element_type = value_request;
    using value_type = Value;

    static constexpr caching_level_type caching_level{
        caching_level_type::none};
    static constexpr bool introspective{false};

    value_request(Value const& value) : value_(value)
    {
    }

    value_request(Value&& value) : value_(std::move(value))
    {
    }

    request_uuid
    get_uuid() const
    {
        // There should be no reason to call this.
        throw not_implemented_error("value_request::get_uuid()");
    }

    // A value request is "trivial": it presents itself as having no
    // subrequests, and no arguments.
    // Thus, accept() becomes a no-op.
    void
    accept(req_visitor_intf& visitor) const
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

    cppcoro::task<Value>
    resolve_sync(local_context_intf& ctx) const
    {
        co_return value_;
    }

    cppcoro::task<Value>
    resolve_async(local_async_context_intf& ctx) const
    {
        co_return value_;
    }

 public:
    // cereal interface
    value_request() = default;

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
auto
rq_value(Value&& value)
{
    return value_request{std::forward<Value>(value)};
}

// operator==() and operator<() are used:
// - For memory cache, ordered map (currently not selected)
// - When a value request is an argument to another request
//
// Value should support operator==() and operator<(), but not necessarily all
// comparison operators demanded by the std::equality_comparable and
// std::totally_ordered concepts.
// The comparison operators that it does implement should comprise a
// consistent ordering relation.
template<typename Value>
bool
operator==(value_request<Value> const& lhs, value_request<Value> const& rhs)
{
    return lhs.get_value() == rhs.get_value();
}

template<typename Value>
bool
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
void
update_unique_hash(unique_hasher& hasher, value_request<Value> const& req)
{
    req.update_hash(hasher);
}

} // namespace cradle

#endif
