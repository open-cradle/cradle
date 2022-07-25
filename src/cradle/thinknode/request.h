#ifndef CRADLE_THINKNODE_REQUEST_H
#define CRADLE_THINKNODE_REQUEST_H

#include <compare>
#include <memory>
#include <optional>
#include <utility>

#include <cereal/archives/binary.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/id.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

namespace detail {

class request_hasher
{
 public:
    template<typename... Args>
    void
    operator()(Args&&... args)
    {
        value_ = combine_hashes(invoke_hash(args)...);
    }

    size_t
    get_value()
    {
        return value_;
    }

 private:
    size_t value_;
};

struct args_comparator
{
    int
    operator()()
    {
        return 0;
    }

    template<typename Arg0, typename... Args>
    requires(std::three_way_comparable<Arg0>) int
    operator()(Arg0&& lhs_arg0, Arg0&& rhs_arg0, Args&&... args)
    {
        int result;
        auto diff = lhs_arg0 <=> rhs_arg0;
        if (diff != 0)
        {
            result = diff < 0 ? -1 : 1;
        }
        else
        {
            result = operator()(std::forward<Args>(args)...);
        }
        return result;
    }

    template<typename Arg0, typename... Args>
    requires(!std::three_way_comparable<Arg0>) int
    operator()(Arg0&& lhs_arg0, Arg0&& rhs_arg0, Args&&... args)
    {
        int result;
        if (lhs_arg0 < rhs_arg0)
        {
            result = -1;
        }
        else if (rhs_arg0 < lhs_arg0)
        {
            result = 1;
        }
        else
        {
            result = operator()(std::forward<Args>(args)...);
        }
        return result;
    }
};

// A request's identity is a combination of:
// - The identity of its class: Base::get_uuid()
// - Its arguments, enumerated in Base::serialize() and Base::compare()
// Most of the functions in this mixin express that identity.
template<typename Base>
class thinknode_request_mixin : public Base, public id_interface
{
 public:
    using element_type = thinknode_request_mixin;
    using value_type = typename Base::value_type;

    template<typename... Args>
    thinknode_request_mixin(Args&&... args) : Base(std::move(args)...)
    {
    }

    bool
    equals(thinknode_request_mixin const& other) const
    {
        if (this == &other)
        {
            return true;
        }
        if (this->get_uuid() != other.get_uuid())
        {
            return false;
        }
        args_comparator cmp;
        return this->compare(cmp, other) == 0;
    }

    bool
    less_than(thinknode_request_mixin const& other) const
    {
        if (this == &other)
        {
            return false;
        }
        auto uuid_diff = this->get_uuid() <=> other.get_uuid();
        if (uuid_diff != 0)
        {
            return uuid_diff < 0;
        }
        args_comparator cmp;
        return this->compare(cmp, other) < 0;
    }

    bool
    equals(id_interface const& other) const override
    {
        auto const& other_id
            = static_cast<thinknode_request_mixin const&>(other);
        return equals(other_id);
    }

    bool
    less_than(id_interface const& other) const override
    {
        auto const& other_id
            = static_cast<thinknode_request_mixin const&>(other);
        return less_than(other_id);
    }

    size_t
    hash() const override
    {
        if (!hash_)
        {
            detail::request_hasher hasher;
            hasher(this->get_uuid());
            const_cast<thinknode_request_mixin*>(this)->serialize(hasher);
            hash_ = hasher.get_value();
        }
        return *hash_;
    }

    // Maybe refactor and cache this hash value too?
    void
    update_hash(unique_hasher& hasher) const override
    {
        std::stringstream ss;
        cereal::BinaryOutputArchive oarchive{ss};
        oarchive(this->get_uuid());
        oarchive(*this); // Calling Base::serialize()
        auto s{ss.str()};
        hasher.encode_value(s.begin(), s.end());
    }

 private:
    mutable std::optional<size_t> hash_;
};

} // namespace detail

template<caching_level_type level, typename Base>
class thinknode_request_container
{
 public:
    using element_type = thinknode_request_container;
    using value_type = typename Base::value_type;
    using impl_type = detail::thinknode_request_mixin<Base>;

    static constexpr caching_level_type caching_level = level;
    // Or depend on Base::get_introspection_title() presence
    static constexpr bool introspective = true;

    template<typename... Args>
    thinknode_request_container(Args... args)
    {
        auto impl{std::make_shared<impl_type>(std::move(args)...)};
        impl_ = impl;
        if constexpr (caching_level != caching_level_type::none)
        {
            captured_id_ = captured_id{impl};
        }
    }

    bool
    equals(thinknode_request_container const& other) const
    {
        return impl_->equals(*other.impl_);
    }

    bool
    less_than(thinknode_request_container const& other) const
    {
        return impl_->less_than(*other.impl_);
    }

    size_t
    hash() const
    {
        return impl_->hash();
    }

    void
    update_hash(unique_hasher& hasher) const
    {
        impl_->update_hash(hasher);
    }

    captured_id const&
    get_captured_id() const
    {
        if constexpr (caching_level == caching_level_type::none)
        {
            throw not_implemented_error(
                "captured_id only available for cached requests");
        }
        return captured_id_;
    }

    cppcoro::task<value_type>
    resolve(thinknode_request_context& ctx) const
    {
        return impl_->resolve(ctx);
    }

    std::string
    get_introspection_title() const requires(introspective)
    {
        return impl_->get_introspection_title();
    }

 private:
    std::shared_ptr<impl_type> impl_;
    captured_id captured_id_;
};

template<typename Value>
class thinknode_request_intf : public id_interface
{
 public:
    virtual ~thinknode_request_intf() = default;

    virtual cppcoro::task<Value>
    resolve(thinknode_request_context& ctx) const = 0;
};

template<typename Base>
class thinknode_request_impl
    : public thinknode_request_intf<typename Base::value_type>,
      Base
{
 public:
    using value_type = typename Base::value_type;

    template<typename... Args>
    thinknode_request_impl(Args&&... args) : Base(std::move(args)...)
    {
    }

    cppcoro::task<value_type>
    resolve(thinknode_request_context& ctx) const override
    {
        return static_cast<Base const*>(this)->resolve(ctx);
    }

    bool
    equals(thinknode_request_impl const& other) const
    {
        if (this == &other)
        {
            return true;
        }
        if (this->get_uuid() != other.get_uuid())
        {
            return false;
        }
        detail::args_comparator cmp;
        return this->compare(cmp, other) == 0;
    }

    bool
    less_than(thinknode_request_impl const& other) const
    {
        if (this == &other)
        {
            return false;
        }
        auto uuid_diff = this->get_uuid() <=> other.get_uuid();
        if (uuid_diff != 0)
        {
            return uuid_diff < 0;
        }
        detail::args_comparator cmp;
        return this->compare(cmp, other) < 0;
    }

    bool
    equals(id_interface const& other) const override
    {
        auto const& other_id
            = static_cast<thinknode_request_impl const&>(other);
        return equals(other_id);
    }

    bool
    less_than(id_interface const& other) const override
    {
        auto const& other_id
            = static_cast<thinknode_request_impl const&>(other);
        return less_than(other_id);
    }

    size_t
    hash() const override
    {
        if (!hash_)
        {
            detail::request_hasher hasher;
            hasher(this->get_uuid());
            const_cast<thinknode_request_impl*>(this)->serialize(hasher);
            hash_ = hasher.get_value();
        }
        return *hash_;
    }

    // Maybe refactor and cache this hash value too?
    void
    update_hash(unique_hasher& hasher) const override
    {
        std::stringstream ss;
        cereal::BinaryOutputArchive oarchive{ss};
        oarchive(this->get_uuid());
        oarchive(*static_cast<Base const*>(this)); // Calling Base::serialize()
        auto s{ss.str()};
        hasher.encode_value(s.begin(), s.end());
    }

 private:
    mutable std::optional<size_t> hash_;
};

template<caching_level_type level, typename Value>
class thinknode_request_erased
{
 public:
    using element_type = thinknode_request_erased;
    using value_type = Value;

    static constexpr caching_level_type caching_level = level;
    // Or depend on Base::get_introspection_title() presence
    static constexpr bool introspective = true;

    thinknode_request_erased(
        std::shared_ptr<thinknode_request_intf<Value>> const& impl)
        : impl_{impl}
    {
        if constexpr (caching_level != caching_level_type::none)
        {
            captured_id_ = captured_id{impl};
        }
    }

    bool
    equals(thinknode_request_erased const& other) const
    {
        return impl_->equals(*other.impl_);
    }

    bool
    less_than(thinknode_request_erased const& other) const
    {
        return impl_->less_than(*other.impl_);
    }

    size_t
    hash() const
    {
        return impl_->hash();
    }

    void
    update_hash(unique_hasher& hasher) const
    {
        impl_->update_hash(hasher);
    }

    captured_id const&
    get_captured_id() const
    {
        if constexpr (caching_level == caching_level_type::none)
        {
            throw not_implemented_error(
                "captured_id only available for cached requests");
        }
        return captured_id_;
    }

    cppcoro::task<Value>
    resolve(thinknode_request_context& ctx) const
    {
        return impl_->resolve(ctx);
    }

    std::string
    get_introspection_title() const requires(introspective)
    {
        return title_;
    }

 private:
    std::string title_;
    std::shared_ptr<thinknode_request_intf<Value>> impl_;
    captured_id captured_id_;
};

} // namespace cradle

#endif
