#ifndef CRADLE_INNER_CORE_TYPE_DEFINITIONS_H
#define CRADLE_INNER_CORE_TYPE_DEFINITIONS_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <cradle/inner/core/exception.h>

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

typedef std::vector<std::uint8_t> byte_vector;

// Owns the data to which a blob refers.
class data_owner
{
 public:
    virtual ~data_owner() = default;

    // Returns a pointer to the data. Throws if not supported.
    virtual std::uint8_t*
    data()
    {
        throw not_implemented_error();
    }

    // true if the data is formed by a memory-mapped file
    virtual bool
    maps_file() const
    {
        return false;
    }

    // If maps_file: absolute path to the memory-mapped file
    virtual std::string
    mapped_file() const
    {
        throw not_implemented_error();
    }

    // If the owned data was modified after this object was created,
    // this function should be called after the modification has completed.
    // If the data is formed by a memory-mapped file, this function will
    // flush memory contents to that file (possibly asynchronously);
    // otherwise, it will be a no-op. A flush will also happen when
    // this object's destructor is called.
    virtual void
    on_write_completed()
    {
    }
};

// A blob is an immutable sequence of bytes: once constructed or deserialized,
// it must not be changed anymore.
class blob
{
 public:
    // Should be followed up by a reset()
    blob() = default;

    // To be used for static data (no owner)
    blob(std::byte const* data, std::size_t size) : data_{data}, size_{size}
    {
    }

    blob(
        std::shared_ptr<data_owner> owner,
        std::byte const* data,
        std::size_t size)
        : owner_{std::move(owner)}, data_{data}, size_{size}
    {
    }

    // Intended for deserialization only, on an empty object
    void
    reset(
        std::shared_ptr<data_owner> owner,
        std::byte const* data,
        std::size_t size)
    {
        assert(!data_);
        owner_ = std::move(owner);
        data_ = data;
        size_ = size;
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

    data_owner const*
    owner() const
    {
        return owner_ ? &*owner_ : nullptr;
    }

    data_owner const*
    mapped_file_data_owner() const
    {
        return owner_ && owner_->maps_file() ? &*owner_ : nullptr;
    }

 private:
    std::shared_ptr<data_owner> owner_;
    std::byte const* data_{nullptr};
    std::size_t size_{0};
};

} // namespace cradle

#endif
