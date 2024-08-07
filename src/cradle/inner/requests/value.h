#ifndef CRADLE_INNER_REQUESTS_VALUE_H
#define CRADLE_INNER_REQUESTS_VALUE_H

#include <utility>

#include <cppcoro/task.hpp>
#include <msgpack.hpp>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/inner/encodings/msgpack_packer.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/uuid.h>

namespace cradle {

class cache_record_lock;

/**
 * Request for an immediate value. No caching, no introspection.
 * Satisfies concept Request.
 */
template<typename Value>
class value_request
{
 public:
    using element_type = value_request;
    using value_type = Value;

    static constexpr bool is_proxy{false};
    static constexpr bool retryable{false};

    value_request(Value const& value) : value_(value)
    {
    }

    value_request(Value&& value) : value_(std::move(value))
    {
    }

    caching_level_type
    get_caching_level() const
    {
        return caching_level_type::none;
    }

    bool
    is_introspective() const
    {
        return false;
    }

    std::string
    get_introspection_title() const
    {
        throw not_implemented_error{
            "value_request::get_introspection_title()"};
    }

    std::unique_ptr<request_essentials>
    get_essentials() const
    {
        // no uuid, no title
        return nullptr;
    }

    // A value request is "trivial": it presents itself as having no
    // subrequests, and no arguments.
    // Thus, accept() becomes a no-op.
    void
    accept(req_visitor_intf& visitor) const
    {
    }

    Value
    get_value() const&
    {
        return value_;
    }

    Value&&
    get_value() &&
    {
        return std::move(value_);
    }

    void
    update_hash(unique_hasher& hasher) const
    {
        update_unique_hash(hasher, value_);
    }

    cppcoro::task<Value>
    resolve(local_context_intf& ctx, cache_record_lock* lock_ptr) const
    {
        co_return value_;
    }

 public:
    // cereal + msgpack interface
    value_request() = default;

    template<typename Archive>
    void
    serialize(Archive& archive)
    {
        archive(value_);
    }

    void
    msgpack_pack(msgpack_packer_base& base_packer) const
    {
        base_packer.pack(value_);
    }

    void
    msgpack_unpack(msgpack::object const& msgpack_obj)
    {
        msgpack_obj.convert(value_);
    }

 private:
    Value value_;
};

// Tests whether T is a value_request instantiation
template<typename T>
struct is_value_request : std::false_type
{
};

template<typename Value>
struct is_value_request<value_request<Value>> : std::true_type
{
};

template<typename T>
inline constexpr bool is_value_request_v = is_value_request<T>::value;

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
