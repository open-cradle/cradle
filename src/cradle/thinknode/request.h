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
#include <cradle/inner/requests/cereal.h>
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
// - The identity of its class, e.g. Base::get_uuid()
// - Its arguments, e.g. Base's hash(), save(), load(), compare()
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

    // *this and other are the same type
    bool
    equals(thinknode_request_mixin const& other) const
    {
        assert(this->get_uuid() == other.get_uuid());
        if (this == &other)
        {
            return true;
        }
        args_comparator cmp;
        return this->compare(cmp, other) == 0;
    }

    // *this and other are the same type
    bool
    less_than(thinknode_request_mixin const& other) const
    {
        assert(this->get_uuid() == other.get_uuid());
        if (this == &other)
        {
            return false;
        }
        args_comparator cmp;
        return this->compare(cmp, other) < 0;
    }

    // The caller has verified that *this and other are the same type.
    bool
    equals(id_interface const& other) const override
    {
        assert(dynamic_cast<thinknode_request_mixin const*>(&other));
        auto const& other_id
            = static_cast<thinknode_request_mixin const&>(other);
        return equals(other_id);
    }

    // The caller has verified that *this and other are the same type.
    bool
    less_than(id_interface const& other) const override
    {
        assert(dynamic_cast<thinknode_request_mixin const*>(&other));
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
            Base::hash(hasher);
            hash_ = hasher.get_value();
        }
        return *hash_;
    }

    void
    update_hash(unique_hasher& hasher) const override
    {
        if (!have_unique_hash_)
        {
            calc_unique_hash();
        }
        hasher.combine(unique_hash_);
    }

 public:
    // Interface for cereal

    thinknode_request_mixin() : Base()
    {
    }

 private:
    mutable std::optional<size_t> hash_;
    mutable unique_hasher::result_t unique_hash_;
    mutable bool have_unique_hash_{false};

    void
    calc_unique_hash() const
    {
        unique_functor functor;
        functor(this->get_uuid());
        Base::hash(functor);
        functor.get_result(unique_hash_);
        have_unique_hash_ = true;
    }
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
    static constexpr bool introspective = true;

    template<typename... Args>
    thinknode_request_container(Args... args)
        : impl_{std::make_shared<impl_type>(std::move(args)...)}
    {
        init_captured_id();
    }

    // *this and other are the same type
    bool
    equals(thinknode_request_container const& other) const
    {
        return impl_->equals(*other.impl_);
    }

    // *this and other are the same type
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

    request_uuid
    get_uuid() const
    {
        return impl_->get_uuid();
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

 public:
    // Interface for cereal

    thinknode_request_container() = default;

    template<typename Archive>
    void
    save(Archive& archive) const
    {
        archive(
            cereal::make_nvp("impl", impl_));
    }

    template<typename Archive>
    void
    load(Archive& archive)
    {
        archive(impl_);
        init_captured_id();
    }

 private:
    std::shared_ptr<impl_type> impl_;
    captured_id captured_id_;

    void
    init_captured_id()
    {
        if constexpr (caching_level != caching_level_type::none)
        {
            captured_id_ = captured_id{impl_};
        }
    }
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
      public Base
{
 public:
    using value_type = typename Base::value_type;
    using this_type = thinknode_request_impl;
    using intf_type = thinknode_request_intf<value_type>;

    // Not to be called when deserializing
    template<typename... Args>
    thinknode_request_impl(Args&&... args) : Base(std::move(args)...)
    {
        register_polymorphic_type<this_type, intf_type>(this->get_uuid());
    }

    cppcoro::task<value_type>
    resolve(thinknode_request_context& ctx) const override
    {
        return static_cast<Base const*>(this)->resolve(ctx);
    }

    // *this and other are the same type
    bool
    equals(thinknode_request_impl const& other) const
    {
        if (this == &other)
        {
            return true;
        }
        detail::args_comparator cmp;
        return this->compare(cmp, other) == 0;
    }

    // *this and other are the same type
    bool
    less_than(thinknode_request_impl const& other) const
    {
        if (this == &other)
        {
            return false;
        }
        detail::args_comparator cmp;
        return this->compare(cmp, other) < 0;
    }

    // Caller promises that *this and other are the same type.
    bool
    equals(id_interface const& other) const override
    {
        assert(dynamic_cast<thinknode_request_impl const*>(&other));
        auto const& other_id
            = static_cast<thinknode_request_impl const&>(other);
        return equals(other_id);
    }

    // Caller promises that *this and other are the same type.
    bool
    less_than(id_interface const& other) const override
    {
        assert(dynamic_cast<thinknode_request_impl const*>(&other));
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
            Base::hash(hasher);
            hash_ = hasher.get_value();
        }
        return *hash_;
    }

    void
    update_hash(unique_hasher& hasher) const override
    {
        if (!have_unique_hash_)
        {
            calc_unique_hash();
        }
        hasher.combine(unique_hash_);
    }

 private:
    // cereal interface. save() and load() are in Base.

    friend class cereal::access;
    thinknode_request_impl() : Base()
    {
    }

 private:
    mutable std::optional<size_t> hash_;
    mutable unique_hasher::result_t unique_hash_;
    mutable bool have_unique_hash_{false};

    void
    calc_unique_hash() const
    {
        unique_functor functor;
        functor(this->get_uuid());
        Base::hash(functor);
        functor.get_result(unique_hash_);
        have_unique_hash_ = true;
    }
};

template<caching_level_type level, typename Value>
class thinknode_request_erased
{
 public:
    using element_type = thinknode_request_erased;
    using value_type = Value;

    static constexpr caching_level_type caching_level = level;
    // Or depend on Base::get_introspection_title() presence
    static constexpr bool introspective = false;

    thinknode_request_erased(
        std::shared_ptr<thinknode_request_intf<Value>> const& impl)
        : impl_{impl}
    {
        init_captured_id();
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

    request_uuid
    get_uuid() const
    {
        return impl_->get_uuid();
    }

    cppcoro::task<Value>
    resolve(thinknode_request_context& ctx) const
    {
        return impl_->resolve(ctx);
    }

    std::string
    get_introspection_title() const requires(introspective)
    {
        return impl_->get_introspection_title();
    }

 public:
    // cereal-related interface

    thinknode_request_erased() = default;

    template<typename Archive>
    void
    save(Archive& archive) const
    {
        // Trust archive to be save-only
        const_cast<thinknode_request_erased*>(this)->load_save(archive);
    }

    template<typename Archive>
    void
    load(Archive& archive)
    {
        load_save(archive);
        init_captured_id();
    }

 private:
    template<typename Archive>
    void
    load_save(Archive& archive)
    {
        archive(cereal::make_nvp("impl", impl_));
    }

 private:
    std::shared_ptr<thinknode_request_intf<Value>> impl_;
    captured_id captured_id_;

    void
    init_captured_id()
    {
        if constexpr (caching_level != caching_level_type::none)
        {
            captured_id_ = captured_id{impl_};
        }
    }
};

} // namespace cradle

#endif
