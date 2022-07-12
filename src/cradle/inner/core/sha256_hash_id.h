#ifndef CRADLE_INNER_CORE_SHA256_HASH_ID_H
#define CRADLE_INNER_CORE_SHA256_HASH_ID_H

#include <tuple>

#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/id.h>
#include <cradle/inner/core/unique_hash.h>

namespace cradle {

// A generic id_interface implementation, representing an arbitrary sequence
// of arguments.
template<class... Args>
struct sha256_hashed_id : id_interface
{
    // Seems to be used in test code only
    sha256_hashed_id()
    {
    }

    sha256_hashed_id(std::tuple<Args...> args) : args_(std::move(args))
    {
    }

    bool
    equals(id_interface const& other) const override
    {
        sha256_hashed_id const& other_id
            = static_cast<sha256_hashed_id const&>(other);
        return args_ == other_id.args_;
    }

    bool
    less_than(id_interface const& other) const override
    {
        sha256_hashed_id const& other_id
            = static_cast<sha256_hashed_id const&>(other);
        return args_ < other_id.args_;
    }

    // aka hash_value()
    // Collisions are allowed.
    size_t
    hash() const override
    {
        return std::apply(
            [](auto... args) { return combine_hashes(invoke_hash(args)...); },
            args_);
    }

    // Update hasher's hash according to this ID.
    void
    update_hash(unique_hasher& hasher) const override
    {
        std::apply(
            [&hasher](auto&&... args) {
                (update_unique_hash(hasher, args), ...);
            },
            args_);
    }

 private:
    std::tuple<Args...> args_;
};

// Used in test code only
template<class... Args>
sha256_hashed_id<Args...>
make_sha256_hashed_id(Args... args)
{
    return sha256_hashed_id<Args...>(std::make_tuple(std::move(args)...));
}

template<class... Args>
captured_id
make_captured_sha256_hashed_id(Args... args)
{
    return captured_id{
        new sha256_hashed_id<Args...>(std::make_tuple(std::move(args)...))};
}

} // namespace cradle

#endif
