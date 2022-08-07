#ifndef CRADLE_INNER_CORE_ID_H
#define CRADLE_INNER_CORE_ID_H

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>

#include <boost/lexical_cast.hpp>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/unique_hash.h>

// This file implements the concept of IDs in CRADLE.

namespace cradle {

// id_interface defines the interface required of all ID types.
struct id_interface
{
    virtual ~id_interface() = default;

    // Given another ID of the same type, return true iff it's equal to this
    // one.
    virtual bool
    equals(id_interface const& other) const = 0;

    // Given another ID of the same type, return true iff it's less than this
    // one.
    virtual bool
    less_than(id_interface const& other) const = 0;

    // Generate a hash of the ID. This need not be unique over co-existing
    // objects.
    virtual size_t
    hash() const = 0;

    // Update hasher's hash according to this ID.
    virtual void
    update_hash(unique_hasher& hasher) const = 0;
};

template<>
inline std::size_t
invoke_hash(id_interface const& x)
{
    return x.hash();
}

// The following convert the interface of the ID operations into the usual form
// that one would expect, as free functions.

inline bool
operator==(id_interface const& a, id_interface const& b)
{
    // Apparently it's faster to compare the name pointers for equality before
    // resorting to actually comparing the typeid objects themselves.
    return (typeid(a).name() == typeid(b).name() || typeid(a) == typeid(b))
           && a.equals(b);
}

inline bool
operator!=(id_interface const& a, id_interface const& b)
{
    return !(a == b);
}

bool
operator<(id_interface const& a, id_interface const& b);

// The following allow the use of IDs as keys in a map or unordered_map.
// The IDs are stored separately as captured_ids in the mapped values and
// pointers are used as keys. This allows searches to be done on pointers to
// other IDs.

struct id_interface_pointer_less_than_test
{
    bool
    operator()(id_interface const* a, id_interface const* b) const
    {
        return *a < *b;
    }
};

struct id_interface_pointer_equality_test
{
    bool
    operator()(id_interface const* a, id_interface const* b) const
    {
        return *a == *b;
    }
};

struct id_interface_pointer_hash
{
    size_t
    operator()(id_interface const* id) const
    {
        return id->hash();
    }
};

// captured_id is used to capture an ID for long-term storage (beyond the point
// where the id_interface reference will be valid).
struct captured_id
{
    captured_id() = default;
    captured_id(captured_id const& other) = default;
    captured_id(captured_id&& other) noexcept = default;

    // Taking ownership of id, which should have been created by new()
    explicit captured_id(id_interface* id) : id_{id}
    {
    }

    // The aliasing constructor; ownership information shared with other
    captured_id(std::shared_ptr<id_interface> const& other)
        : id_{other, other.get()}
    {
    }

    captured_id&
    operator=(captured_id const& other)
        = default;
    captured_id&
    operator=(captured_id&& other) noexcept = default;
    void
    clear()
    {
        id_.reset();
    }
    bool
    is_initialized() const
    {
        return id_ ? true : false;
    }
    id_interface const&
    operator*() const
    {
        return *id_;
    }
    id_interface const*
    operator->() const
    {
        return &*id_;
    }
    bool
    matches(id_interface const& id) const
    {
        return id_ && *id_ == id;
    }
    friend void
    swap(captured_id& a, captured_id& b) noexcept
    {
        swap(a.id_, b.id_);
    }
    size_t
    hash() const
    {
        return id_ ? id_->hash() : 0;
    }

 private:
    std::shared_ptr<id_interface> id_;
};
bool
operator==(captured_id const& a, captured_id const& b);
bool
operator!=(captured_id const& a, captured_id const& b);
bool
operator<(captured_id const& a, captured_id const& b);

// ref(id) wraps a reference to an id_interface so that it can be combined.
struct id_ref : id_interface
{
    id_ref() : id_(0), ownership_()
    {
    }

    id_ref(id_interface const& id) : id_(&id), ownership_()
    {
    }

    bool
    equals(id_interface const& other) const override
    {
        id_ref const& other_id = static_cast<id_ref const&>(other);
        return *id_ == *other_id.id_;
    }

    bool
    less_than(id_interface const& other) const override
    {
        id_ref const& other_id = static_cast<id_ref const&>(other);
        return *id_ < *other_id.id_;
    }

    size_t
    hash() const override
    {
        return id_->hash();
    }

    void
    update_hash(unique_hasher& hasher) const override
    {
        id_->update_hash(hasher);
    }

 private:
    id_interface const* id_;
    std::shared_ptr<id_interface> ownership_;
};
inline id_ref
ref(id_interface const& id)
{
    return id_ref(id);
}

// simple_id<Value> takes a regular type (Value) and implements id_interface
// for it. The type Value must be copyable and comparable for equality and
// ordering (i.e., supply == and < operators).
// Clang doesn't like ordered function pointer comparisons but it seems safe in
// this context.
template<class Value>
struct simple_id : id_interface
{
    simple_id()
    {
    }

    simple_id(Value value) : value_(value)
    {
    }

    Value const&
    value() const
    {
        return value_;
    }

    bool
    equals(id_interface const& other) const override
    {
        simple_id const& other_id = static_cast<simple_id const&>(other);
        return value_ == other_id.value_;
    }

    bool
    less_than(id_interface const& other) const override
    {
        simple_id const& other_id = static_cast<simple_id const&>(other);
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wordered-compare-function-pointers"
#endif
        return value_ < other_id.value_;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    }

    size_t
    hash() const override
    {
        return invoke_hash(value_);
    }

    void
    update_hash(unique_hasher& hasher) const override
    {
        update_unique_hash(hasher, value_);
    }

    Value value_;
};

// make_id(value) creates a simple_id with the given value.
template<class Value>
simple_id<Value>
make_id(Value value)
{
    return simple_id<Value>(value);
}

// make_captured_id(value) creates a captured simple_id with the given value.
template<class Value>
captured_id
make_captured_id(Value value)
{
    return captured_id(new simple_id<Value>(value));
}

// simple_id_by_reference is like simple_id but takes a pointer to the value.
// The value is never copied.
template<class Value>
struct simple_id_by_reference : id_interface
{
    simple_id_by_reference() : value_(0), storage_()
    {
    }

    simple_id_by_reference(Value const* value) : value_(value), storage_()
    {
    }

    bool
    equals(id_interface const& other) const override
    {
        simple_id_by_reference const& other_id
            = static_cast<simple_id_by_reference const&>(other);
        return *value_ == *other_id.value_;
    }

    bool
    less_than(id_interface const& other) const override
    {
        simple_id_by_reference const& other_id
            = static_cast<simple_id_by_reference const&>(other);
        return *value_ < *other_id.value_;
    }

    size_t
    hash() const override
    {
        return invoke_hash(*value_);
    }

    void
    update_hash(unique_hasher& hasher) const override
    {
        update_unique_hash(hasher, *value_);
    }

 private:
    Value const* value_;
    std::shared_ptr<Value> storage_;
};

// make_id_by_reference(value) creates a simple_id_by_reference for :value.
template<class Value>
simple_id_by_reference<Value>
make_id_by_reference(Value const& value)
{
    return simple_id_by_reference<Value>(&value);
}

// id_pair implements the ID interface for a pair of IDs.
template<class Id0, class Id1>
struct id_pair : id_interface
{
    id_pair()
    {
    }

    id_pair(Id0 id0, Id1 id1) : id0_(std::move(id0)), id1_(std::move(id1))
    {
    }

    bool
    equals(id_interface const& other) const override
    {
        id_pair const& other_id = static_cast<id_pair const&>(other);
        return id0_.equals(other_id.id0_) && id1_.equals(other_id.id1_);
    }

    bool
    less_than(id_interface const& other) const override
    {
        id_pair const& other_id = static_cast<id_pair const&>(other);
        return id0_.less_than(other_id.id0_)
               || (id0_.equals(other_id.id0_)
                   && id1_.less_than(other_id.id1_));
    }

    size_t
    hash() const override
    {
        return id0_.hash() ^ id1_.hash();
    }

    void
    update_hash(unique_hasher& hasher) const override
    {
        id0_.update_hash(hasher);
        id1_.update_hash(hasher);
    }

 private:
    Id0 id0_;
    Id1 id1_;
};

// combine_ids(id0, id1) combines id0 and id1 into a single ID pair.
template<class Id0, class Id1>
auto
combine_ids(Id0 id0, Id1 id1)
{
    return id_pair<Id0, Id1>(std::move(id0), std::move(id1));
}

// Combine more than two IDs into nested pairs.
template<class Id0, class Id1, class... Rest>
auto
combine_ids(Id0 id0, Id1 id1, Rest... rest)
{
    return combine_ids(
        combine_ids(std::move(id0), std::move(id1)), std::move(rest)...);
}

// Allow combine_ids() to take a single argument for variadic purposes.
template<class Id0>
auto
combine_ids(Id0 id0)
{
    return id0;
}

// null_id can be used when you have nothing to identify.
struct null_id_type
{
};
static simple_id<null_id_type*> const null_id(nullptr);

inline void
update_unique_hash(unique_hasher& hasher, null_id_type* val)
{
}

// unit_id can be used when there is only possible identify.
struct unit_id_type
{
};
static simple_id<unit_id_type*> const unit_id(nullptr);

inline void
update_unique_hash(unique_hasher& hasher, unit_id_type* val)
{
}

} // namespace cradle

#endif
