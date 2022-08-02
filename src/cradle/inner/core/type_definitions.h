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

    // cereal support
    template<typename Archive>
    void
    serialize(Archive& archive)
    {
        archive(size_, data_);
    }

 private:
    std::shared_ptr<std::byte const> data_;
    std::size_t size_;
};

} // namespace cradle

#endif
