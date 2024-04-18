#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_DEMO_CLASS_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_DEMO_CLASS_H

// A simple demo class, showing what is needed to embed it in CRADLE.

#include <cstddef>

#include <msgpack.hpp>

#include <cradle/inner/encodings/cereal_value.h>

namespace cradle {

class unique_hasher;

class demo_class
{
 public:
    // Default constructor needed for deserialization
    demo_class()
    {
    }

    demo_class(int x, int y)
    {
        x_ = x;
        y_ = y;
    }

    int
    get_x() const
    {
        return x_;
    }

    int
    get_y() const
    {
        return y_;
    }

    // private:
    int x_{};
    int y_{};

    // Provide msgpack serialization in the form of a two-element array
    // containing x_ and y_. Some drawbacks:
    // - Intrusive
    // - Member variables must be public
    // - Maybe inflexible
    // Still, this is the official solution, and it's really simple if it
    // suffices.
    MSGPACK_DEFINE(x_, y_)
};

// Needed if demo_class is cached
inline constexpr std::size_t
deep_sizeof(demo_class const& val)
{
    return sizeof(int);
}

// Needed if demo_class is cached
bool
operator==(demo_class const& a, demo_class const& b);

// Needed if demo_class is cached
bool
operator<(demo_class const& a, demo_class const& b);

// Needed if demo_class is cached
std::size_t
hash_value(demo_class const& val);

// Needed if demo_class is cached
void
update_unique_hash(unique_hasher& hasher, demo_class const& val);

// Needed if demo_class is used as argument to a function_request.
template<>
struct serializable_via_cereal<demo_class>
{
    static constexpr bool value = true;
};

} // namespace cradle

#endif
