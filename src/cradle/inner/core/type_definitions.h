#ifndef CRADLE_INNER_CORE_TYPE_DEFINITIONS_H
#define CRADLE_INNER_CORE_TYPE_DEFINITIONS_H

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <cereal/types/memory.hpp>

#include <boost/cstdint.hpp>

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

typedef std::vector<boost::uint8_t> byte_vector;

struct blob
{
    blob() : size_(0)
    {
    }

    blob(std::shared_ptr<std::byte const> data, std::size_t size)
        : data_(std::move(data)), size_(size)
    {
    }

    std::byte const*
    data() const
    {
        return data_.get();
    }

    std::size_t
    size() const
    {
        return size_;
    }

 public:
    // cereal support
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
        archive.saveBinaryValue(data(), size(), "blob");
    }

    template<typename Archive>
    void
    load(Archive& archive)
    {
        // It's somewhat redundant to serialize the size as it's implied by
        // the base64 string, but the array passed to loadBinaryValue()
        // must have the appropriate size.
        archive(size_);
        auto ptr = std::make_shared<std::vector<std::byte>>();
        ptr->reserve(size_);
        auto data = ptr->data();
        archive.loadBinaryValue(data, size_);
        data_ = std::shared_ptr<std::byte const>(std::move(ptr), data);
    }

 private:
    std::shared_ptr<std::byte const> data_;
    std::size_t size_;
};

} // namespace cradle

#endif
