#ifndef CRADLE_INNER_CORE_TYPE_DEFINITIONS_H
#define CRADLE_INNER_CORE_TYPE_DEFINITIONS_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <cereal/types/memory.hpp>

namespace cradle {

typedef std::nullopt_t none_t;
inline constexpr std::nullopt_t none(std::nullopt);

// some(x) creates an optional of the proper type with the value of :x.
template<class T>
auto
some(T&& x)
{
    return std::optional<std::remove_reference_t<T>>(std::forward<T>(x));
}

// TODO consider making this a template where T can be any byte-like type
typedef std::vector<std::uint8_t> byte_vector;

// Owns the data to which a blob refers.
class data_owner
{
 public:
    virtual ~data_owner() = default;
};

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

// A blob is a sequence of bytes.
// A blob, once constructed or deserialized, is immutable.
class blob
{
 public:
    blob() = default;

    // To be used for static data (no owner)
    blob(std::byte const* data, std::size_t size) : data_{data}, size_{size}
    {
    }

    blob(
        std::shared_ptr<data_owner> const& owner,
        std::byte const* data,
        std::size_t size)
        : owner_{owner}, data_{data}, size_{size}
    {
    }

    std::byte const*
    data() const
    {
        return data_;
    }

    std::size_t
    size() const
    {
        return size_;
    }

 public:
    // cereal support
    // TODO move out cereal support to plugin
    //
    // A blob will typically contain binary data. cereal offers
    // saveBinaryValue() and loadBinaryValue() that cause binary data to be
    // stored as base64 in JSON and XML. JSON stores non-printable bytes
    // as e.g. "\u0001" (500% overhead), so base64 (33% overhead) might be
    // more efficient.
    template<typename Archive>
    void
    save(Archive& archive) const
    {
        archive(cereal::make_nvp("size", size_));
        archive.saveBinaryValue(data_, size_, "blob");
    }

    template<typename Archive>
    void
    load(Archive& archive)
    {
        // TODO serialize blob subtypes
        // It's somewhat redundant to serialize the size as it's implied by
        // the base64 string, but the array passed to loadBinaryValue()
        // must have the appropriate size.
        archive(cereal::make_nvp("size", size_));
        // TODO use make_shared_buffer() once in scope
        auto owner{std::make_shared<byte_vector_owner>(byte_vector(size_))};
        archive.loadBinaryValue(owner->data(), size_, "blob");
        owner_ = owner;
        data_ = owner->bytes();
    }

 private:
    std::shared_ptr<data_owner> owner_;
    std::byte const* data_{nullptr};
    std::size_t size_{0};
};

} // namespace cradle

#endif
