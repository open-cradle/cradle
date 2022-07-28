#ifndef CRADLE_INNER_CORE_UNIQUE_HASH_H
#define CRADLE_INNER_CORE_UNIQUE_HASH_H

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <string>
#include <typeinfo>

#include <picosha2.h>

#include <cradle/inner/core/type_definitions.h>

namespace cradle {

struct id_interface;

// Get a string that is unique for the given ID (based on its hash).
std::string
get_unique_string(id_interface const& id);

// Creates a cryptographic-strength hash value that should prevent collisions
// between different items written to the disk cache.
// Collisions between values that happen to have the same bitwise
// representation are prevented by prefixing them with their type.
class unique_hasher
{
 public:
    using byte_t = picosha2::byte_t;
    static constexpr size_t result_size = 32;

    struct result_t
    {
        byte_t bytes[result_size];
        bool initialized{false};
    };

    template<typename T>
    void
    encode_type()
    {
        assert(!finished_);
        // TODO need unique type name
        // From https://en.cppreference.com/w/cpp/types/type_info/name
        // The returned string can be identical for several types and change
        // between invocations of the same program.
        // Maybe replace with type traits yielding a 1-byte value?
        const char* name = typeid(T).name();
        impl_.process(name, name + strlen(name));
    }

    void
    encode_bytes(byte_t const* begin, byte_t const* end)
    {
        assert(!finished_);
        impl_.process(begin, end);
    }

    void
    encode_bytes(std::byte const* begin, std::byte const* end)
    {
        encode_bytes(
            reinterpret_cast<byte_t const*>(begin),
            reinterpret_cast<byte_t const*>(end));
    }

    void
    encode_bytes(char const* begin, char const* end)
    {
        encode_bytes(
            reinterpret_cast<byte_t const*>(begin),
            reinterpret_cast<byte_t const*>(end));
    }

    // Update from a partial hash.
    void
    combine(result_t const& partial);

    void
    get_result(result_t& result);

    std::string
    get_string();

 private:
    void
    finish();

    picosha2::hash256_one_by_one impl_;
    bool finished_{false};
};

template<typename T>
requires std::integral<T> || std::floating_point<T>
void
update_unique_hash(unique_hasher& hasher, T val)
{
    hasher.encode_type<T>();
    char const* p = reinterpret_cast<char const*>(&val);
    hasher.encode_bytes(p, p + sizeof(val));
}

void
update_unique_hash(unique_hasher& hasher, std::string const& val);

void
update_unique_hash(unique_hasher& hasher, blob const& val);

// A wrapper around unique_hasher, transforming it into something that can be
// passed to cereal's serialize().
class unique_functor
{
 public:
    template<typename Arg0, typename... Args>
    void
    operator()(Arg0&& arg0, Args&&... args)
    {
        update_unique_hash(hasher_, std::forward<Arg0>(arg0));
        if constexpr (sizeof...(args) > 0)
        {
            operator()(args...);
        }
    }

    void
    get_result(unique_hasher::result_t& result)
    {
        hasher_.get_result(result);
    }

 private:
    unique_hasher hasher_;
};

} // namespace cradle

#endif
