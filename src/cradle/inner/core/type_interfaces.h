#ifndef CRADLE_INNER_CORE_TYPE_INTERFACES_H
#define CRADLE_INNER_CORE_TYPE_INTERFACES_H

#include <array>
#include <concepts>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cradle/inner/core/type_definitions.h>

namespace cradle {

template<typename T>
    requires std::integral<T> || std::floating_point<T>
inline size_t
deep_sizeof(T x)
{
    return sizeof(x);
}

inline size_t
deep_sizeof(std::string const& x)
{
    return sizeof(std::string) + sizeof(char) * x.length();
}

template<class T, size_t N>
size_t
deep_sizeof(std::array<T, N> const& x)
{
    size_t size = 0;
    for (auto const& i : x)
        size += deep_sizeof(i);
    return size;
}

template<class T>
size_t
deep_sizeof(std::optional<T> const& x)
{
    // using cradle::deep_sizeof;
    return sizeof(std::optional<T>) + (x ? deep_sizeof(*x) : 0);
}

template<class T>
size_t
deep_sizeof(std::vector<T> const& x)
{
    size_t size = sizeof(std::vector<T>);
    for (auto const& i : x)
        size += deep_sizeof(i);
    return size;
}

template<class Key, class Value>
size_t
deep_sizeof(std::map<Key, Value> const& x)
{
    size_t size = sizeof(std::map<Key, Value>);
    for (auto const& i : x)
        size += deep_sizeof(i.first) + deep_sizeof(i.second);
    return size;
}

inline size_t
deep_sizeof(blob const& b)
{
    // This ignores the size of the ownership holder, but that's not a big
    // deal.
    return sizeof(blob) + b.size();
}

bool
operator==(blob const& a, blob const& b);

inline bool
operator!=(blob const& a, blob const& b)
{
    return !(a == b);
}

bool
operator<(blob const& a, blob const& b);

template<class T>
std::byte const*
as_bytes(T const* ptr)
{
    return reinterpret_cast<std::byte const*>(ptr);
}

size_t
hash_value(blob const& x);

// Make a blob using a shared_ptr to another type that owns the actual
// content.
template<class OwnedType>
blob
make_blob(std::shared_ptr<OwnedType> ptr, std::byte const* data, size_t size)
{
    // Here we are leveraging shared_ptr's flexibility to provide ownership of
    // another object (of an arbitrary type) while storing a pointer to data
    // inside that object.
    return blob(std::shared_ptr<std::byte const>(std::move(ptr), data), size);
}

// Make a blob that holds a pointer to some statically allocated data.
blob
make_static_blob(std::byte const* data, size_t size);

// Make a blob that holds a pointer to some statically allocated data.
blob
make_string_literal_blob(char const* data);

// Make a blob that holds the contents of the given string.
blob
make_blob(std::string s);

// Make a blob that holds the contents of a byte vector.
blob
make_blob(byte_vector v);

std::ostream&
operator<<(std::ostream& s, blob const& b);

} // namespace cradle

#endif
