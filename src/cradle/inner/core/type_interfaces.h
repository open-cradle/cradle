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

// Blob data owner where the data is stored in a byte_vector
class byte_vector_owner : public data_owner
{
 public:
    byte_vector_owner(byte_vector value) : value_{std::move(value)}
    {
    }

    ~byte_vector_owner() = default;

    std::uint8_t*
    data()
    {
        return value_.data();
    }

    std::byte*
    bytes()
    {
        return reinterpret_cast<std::byte*>(value_.data());
    }

    size_t
    size() const
    {
        return value_.size();
    }

 private:
    byte_vector value_;
};

// Blob data owner where the data is stored in a string
class string_owner : public data_owner
{
 public:
    string_owner(std::string value) : value_{std::move(value)}
    {
    }

    ~string_owner() = default;

    std::byte const*
    data() const
    {
        return as_bytes(value_.c_str());
    }

    size_t
    size() const
    {
        return value_.size();
    }

 private:
    std::string value_;
};

// Make a blob that holds a pointer to some statically allocated data.
blob
make_static_blob(std::byte const* data, size_t size);

// Make a blob that holds a pointer to some statically allocated data.
blob
make_string_literal_blob(char const* data);

// Make a blob that holds the contents of the given string.
blob
make_blob(std::string s);

// Make a blob that holds the contents of a byte vector, where the blob size
// equals the vector size.
blob
make_blob(byte_vector v);

// Make a blob that holds the contents of a byte vector, where the blob size
// may be smaller than the vector size.
blob
make_blob(byte_vector v, std::size_t size);

// Create a data buffer that can be filled and attached to a blob
std::shared_ptr<byte_vector_owner>
make_shared_buffer(std::size_t size);

// Convert to a string that is identical to the blob, byte by byte
std::string
to_string(blob const& x);

// Convert to something informational
std::ostream&
operator<<(std::ostream& s, blob const& b);

} // namespace cradle

#endif
